#include "../uri_utils.hpp"

#include <gtest/gtest.h>

namespace just_audio_windows {
namespace test {

// ── EncodeSpacesInUri ────────────────────────────────────────────────────────

// A plain URL with no spaces should pass through unchanged.
TEST(EncodeSpacesInUri, NoSpaces_ReturnsUnchanged) {
  EXPECT_EQ(EncodeSpacesInUri("https://example.com/audio.mp3"),
            "https://example.com/audio.mp3");
}

// An empty string should stay empty.
TEST(EncodeSpacesInUri, EmptyString_ReturnsEmpty) {
  EXPECT_EQ(EncodeSpacesInUri(""), "");
}

// Literal spaces in a local file path must be encoded as %20.
// This is the core regression for https://github.com/bdlukaa/just_audio_windows/issues/26.
TEST(EncodeSpacesInUri, LiteralSpacesInFilePath_EncodedAsPercent20) {
  EXPECT_EQ(
      EncodeSpacesInUri("file:///C:/Users/My Files/song.mp3"),
      "file:///C:/Users/My%20Files/song.mp3");
}

// Multiple consecutive spaces must each be individually encoded.
TEST(EncodeSpacesInUri, MultipleSpaces_AllEncoded) {
  EXPECT_EQ(
      EncodeSpacesInUri(
          "C:/Users/HP/Downloads/Ve Kamleya Rocky Aur Rani.mp3"),
      "C:/Users/HP/Downloads/Ve%20Kamleya%20Rocky%20Aur%20Rani.mp3");
}

// Already percent-encoded spaces (%20) must NOT be double-encoded.
// This ensures a properly-encoded file:// URI like those produced by
// Dart's Uri.file() is left intact.
TEST(EncodeSpacesInUri, AlreadyEncodedSpaces_NotDoubleEncoded) {
  EXPECT_EQ(
      EncodeSpacesInUri("file:///C:/Users/My%20Files/song.mp3"),
      "file:///C:/Users/My%20Files/song.mp3");
}

// Percent-encoded multi-byte UTF-8 sequences (e.g. U+2019 RIGHT SINGLE
// QUOTATION MARK) must pass through unchanged so that Windows::Foundation::Uri
// can decode them natively.
// This is the core regression for the original apostrophe bug.
TEST(EncodeSpacesInUri, MultiBytePercentEncoded_Unchanged) {
  EXPECT_EQ(
      EncodeSpacesInUri(
          "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee"),
      "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee");
}

// A URL with both unencoded spaces and percent-encoded multi-byte chars:
// spaces must be encoded while the percent-sequences stay intact.
TEST(EncodeSpacesInUri, MixedLiteralSpacesAndPercentEncoded) {
  EXPECT_EQ(
      EncodeSpacesInUri(
          "https://example.com/speech?text=I%E2%80%99d like a coffee"),
      "https://example.com/speech?text=I%E2%80%99d%20like%20a%20coffee");
}

// A URL with only query-string spaces should have them encoded.
TEST(EncodeSpacesInUri, SpaceInQueryString) {
  EXPECT_EQ(
      EncodeSpacesInUri("https://example.com/tts?text=hello world"),
      "https://example.com/tts?text=hello%20world");
}

}  // namespace test
}  // namespace just_audio_windows
