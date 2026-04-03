#pragma once

#include <string>

// Encodes literal space characters in a URI string as "%20" so that
// Windows::Foundation::Uri accepts URIs that contain unencoded spaces
// (e.g. local file paths like "file:///C:/My Files/song.mp3").
//
// Already percent-encoded sequences such as "%20" or "%E2%80%99" are never
// modified, because they contain no literal space character.
inline std::string EncodeSpacesInUri(const std::string& uri) {
  std::string encoded;
  // Reserve worst-case capacity (every char is a space → 3 chars each).
  encoded.reserve(uri.length() * 3);
  for (char c : uri) {
    if (c == ' ') {
      encoded += "%20";
    } else {
      encoded += c;
    }
  }
  return encoded;
}
