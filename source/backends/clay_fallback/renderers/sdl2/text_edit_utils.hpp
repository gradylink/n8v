#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

namespace n8v::detail {

inline size_t utf8CodepointLen(std::string_view s, size_t pos) {
  if (pos >= s.size()) return 0;
  unsigned char c = (unsigned char)s[pos];
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

inline size_t prevCodepointStart(std::string_view s, size_t pos) {
  if (pos == 0) return 0;
  size_t i = pos - 1;
  while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
  return i;
}

inline size_t nextCodepointStart(std::string_view s, size_t pos) { return pos >= s.size() ? s.size() : pos + utf8CodepointLen(s, pos); }

inline bool isWordByte(unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; }

inline size_t wordLeft(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i > 0 && !isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  while (i > 0 && isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  return i;
}

inline size_t wordRight(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i < s.size() && isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  while (i < s.size() && !isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  return i;
}

inline size_t wordStartAt(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i > 0 && isWordByte((unsigned char)s[prevCodepointStart(s, i)])) i = prevCodepointStart(s, i);
  return i;
}

inline size_t wordEndAt(std::string_view s, size_t pos) {
  size_t i = pos;
  while (i < s.size() && isWordByte((unsigned char)s[i])) i = nextCodepointStart(s, i);
  return i;
}

inline std::vector<size_t> codepointByteOffsets(std::string_view s) {
  std::vector<size_t> offsets;
  size_t i = 0;
  while (i < s.size()) {
    offsets.push_back(i);
    i += utf8CodepointLen(s, i);
  }
  offsets.push_back(s.size());
  return offsets;
}

inline size_t realOffsetToDisplayOffset(std::string_view realValue, size_t realByteOffset, bool isPassword) {
  if (!isPassword) return realByteOffset;
  std::vector<size_t> offsets = codepointByteOffsets(realValue);
  for (size_t k = 0; k < offsets.size(); ++k) {
    if (offsets[k] == realByteOffset) return k;
  }
  return offsets.size() - 1;
}

inline size_t displayOffsetToRealOffset(std::string_view realValue, size_t displayByteOffset, bool isPassword) {
  if (!isPassword) return std::min(displayByteOffset, realValue.size());
  std::vector<size_t> offsets = codepointByteOffsets(realValue);
  size_t k = std::min(displayByteOffset, offsets.size() - 1);
  return offsets[k];
}

} // namespace n8v::detail
