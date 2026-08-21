#pragma once

// This must be included before many other Windows headers.
#include <windows.h>

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

// Runs closures on the Flutter platform thread.
//
// WinRT delivers MediaPlayer's events (PlaybackStateChanged, MediaFailed,
// CurrentItemChanged, ItemFailed) on thread-pool threads, but a Flutter platform
// channel may only be written from the platform thread. Writing from another
// thread risks dropped messages and crashes, and the engine logs
//
//   The '...' channel sent a message from native to Flutter on a non-platform
//   thread.
//
// for each one. So anything that reaches an EventSink from a WinRT callback is
// queued here and run on the platform thread instead.
//
// The mechanism is a message-only window owned by this class: work goes on a
// mutex-guarded queue and a message is posted to that window, whose WndProc runs
// on the thread that created it — the platform thread — and drains the queue.
//
// Deliberately NOT `PluginRegistrarWindows::RegisterTopLevelWindowProcDelegate`
// plus a post to the top-level window. That looks like the natural fit and it
// silently does not work: `PostMessage` succeeds, and the delegate is simply
// never invoked for the posted message, so the queue never drains and every
// event is lost. A message-only window with its own WndProc has no such
// dependency.
class PlatformThreadDispatcher {
 public:
  PlatformThreadDispatcher() {
    // This constructor runs during plugin registration, i.e. on the platform
    // thread, so the window created here belongs to that thread and its WndProc
    // is pumped by it.
    platform_thread_id_ = GetCurrentThreadId();

    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = &PlatformThreadDispatcher::WndProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = kWindowClass;
    // A second registration fails with ERROR_CLASS_ALREADY_EXISTS, which is
    // harmless — CreateWindowExW below only needs the class to exist.
    RegisterClassExW(&cls);

    window_ = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                              nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window_) return;
    SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  }

  ~PlatformThreadDispatcher() {
    if (window_) {
      SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
      DestroyWindow(window_);
      window_ = nullptr;
    }
    // Whatever is still queued will never be drained. Drop it rather than run
    // it during teardown, when the players it refers to are going away.
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.clear();
  }

  PlatformThreadDispatcher(const PlatformThreadDispatcher&) = delete;
  PlatformThreadDispatcher& operator=(const PlatformThreadDispatcher&) = delete;

  // Whether work can actually be marshalled. False if the window could not be
  // created; callers then run inline, which is what the plugin did everywhere
  // before this existed.
  bool available() const { return window_ != nullptr; }

  // Whether the caller is already on the platform thread, in which case there is
  // nothing to marshal and the work should just run — that keeps the
  // method-channel paths (seekToItem, seekToPosition) writing synchronously, as
  // they did before, rather than being pushed a message-pump turn later.
  bool on_platform_thread() const {
    return GetCurrentThreadId() == platform_thread_id_;
  }

  void Post(std::function<void()> task) {
    if (!available()) return;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push_back(std::move(task));
    }
    PostMessageW(window_, kRunTasks, 0, 0);
  }

 private:
  static constexpr const wchar_t* kWindowClass =
      L"JustAudioWindowsDispatcherWindow";
  static constexpr UINT kRunTasks = WM_USER + 1;

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam,
                                  LPARAM lparam) {
    if (message == kRunTasks) {
      auto* self = reinterpret_cast<PlatformThreadDispatcher*>(
          GetWindowLongPtrW(hwnd, GWLP_USERDATA));
      if (self) {
        self->Drain();
        return 0;
      }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  void Drain() {
    // Swap the queue out and run outside the lock: a task may post more work,
    // and holding the mutex through it would deadlock.
    std::vector<std::function<void()>> tasks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks.swap(tasks_);
    }
    for (auto& task : tasks) {
      task();
    }
  }

  HWND window_ = nullptr;
  DWORD platform_thread_id_ = 0;
  std::mutex mutex_;
  std::vector<std::function<void()>> tasks_;
};
