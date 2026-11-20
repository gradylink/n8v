#include "kitty_graphics.hpp"

#include "kitty_diacritics.hpp"

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace n8v::detail {
namespace {

constexpr uint32_t kFirstId = 0x4e8000;
constexpr size_t kChunkChars = 4096;
constexpr int kPixelsPerCellX = 20;
constexpr int kPixelsPerCellY = 40;

void appendUtf8(std::string &out, uint32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0 | (cp >> 6));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += (char)(0xE0 | (cp >> 12));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | (cp >> 18));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

std::string base64(const uint8_t *data, size_t size) {
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((size + 2) / 3 * 4);
  for (size_t i = 0; i < size; i += 3) {
    const uint32_t a = data[i], b = i + 1 < size ? data[i + 1] : 0, c = i + 2 < size ? data[i + 2] : 0;
    const uint32_t triple = (a << 16) | (b << 8) | c;
    out += alphabet[(triple >> 18) & 63];
    out += alphabet[(triple >> 12) & 63];
    out += i + 1 < size ? alphabet[(triple >> 6) & 63] : '=';
    out += i + 2 < size ? alphabet[triple & 63] : '=';
  }
  return out;
}

void writeAll(const std::string &bytes) {
  size_t done = 0;
  while (done < bytes.size()) {
    const ssize_t n = ::write(STDOUT_FILENO, bytes.data() + done, bytes.size() - done);
    if (n <= 0) return;
    done += (size_t)n;
  }
}

bool envIs(const char *name, const char *value) {
  const char *v = std::getenv(name);
  return v && std::strcmp(v, value) == 0;
}

bool envContains(const char *name, const char *needle) {
  const char *v = std::getenv(name);
  return v && std::strstr(v, needle);
}

} // namespace

ImageProtocol detectImageProtocol() {
  if (envIs("N8V_TUI_IMAGES", "blocks")) return ImageProtocol::Blocks;
  if (envIs("N8V_TUI_IMAGES", "kitty")) return ImageProtocol::Kitty;
  if (envIs("N8V_TUI_IMAGES", "kitty-direct")) return ImageProtocol::KittyDirect;
  if (std::getenv("TMUX")) return ImageProtocol::Blocks;
  if (std::getenv("KITTY_WINDOW_ID") || envIs("TERM", "xterm-kitty") || envIs("TERM", "xterm-ghostty") || envIs("TERM_PROGRAM", "ghostty")) return ImageProtocol::Kitty;
  if (envIs("TERM_PROGRAM", "WezTerm") || std::getenv("WEZTERM_PANE")) return ImageProtocol::KittyDirect;
  return ImageProtocol::Blocks;
}

std::string KittyGraphics::placeholder(int row, int col) {
  std::string cell;
  appendUtf8(cell, 0x10EEEE);
  appendUtf8(cell, kKittyDiacritics[std::clamp<size_t>((size_t)std::max(row, 0), 0, kKittyDiacriticCount - 1)]);
  appendUtf8(cell, kKittyDiacritics[std::clamp<size_t>((size_t)std::max(col, 0), 0, kKittyDiacriticCount - 1)]);
  return cell;
}

void KittyGraphics::idToRgb(uint32_t id, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = (uint8_t)((id >> 16) & 0xFF);
  g = (uint8_t)((id >> 8) & 0xFF);
  b = (uint8_t)(id & 0xFF);
}

uint32_t KittyGraphics::ensure(const DecodedImage *image, int cols, int rows) {
  if (!image || !image->rgba || image->width <= 0 || image->height <= 0) return 0;
  if (cols <= 0 || rows <= 0 || (size_t)cols > kKittyDiacriticCount || (size_t)rows > kKittyDiacriticCount) return 0;

  const auto key = std::make_tuple(image, cols, rows);
  if (auto it = ids_.find(key); it != ids_.end()) {
    lastUsed_[it->second] = frame_;
    return it->second;
  }

  const DecodedImage *source = image;
  const double scale = std::min({1.0, (double)cols * kPixelsPerCellX / image->width, (double)rows * kPixelsPerCellY / image->height});
  if (scale < 1.0) {
    const int w = std::max(1, (int)std::lround(image->width * scale));
    const int h = std::max(1, (int)std::lround(image->height * scale));
    if (const DecodedImage *scaled = getOrScaleImage(image, w, h)) source = scaled;
  }

  const uint32_t id = kFirstId + next_++;
  const std::string payload = base64(source->rgba, (size_t)source->width * source->height * 4);
  std::string out;
  for (size_t pos = 0; pos < payload.size() || pos == 0; pos += kChunkChars) {
    const bool more = pos + kChunkChars < payload.size();
    out += "\x1b_G";
    if (pos == 0) {
      const std::string common = "q=2,f=32,i=" + std::to_string(id) + ",s=" + std::to_string(source->width) + ",v=" + std::to_string(source->height) + ",";
      if (protocol_ == ImageProtocol::KittyDirect) out += "a=t," + common;
      else out += "a=T,U=1," + common + "c=" + std::to_string(cols) + ",r=" + std::to_string(rows) + ",";
    }
    out += more ? "m=1;" : "m=0;";
    out.append(payload, pos, kChunkChars);
    out += "\x1b\\";
    if (!more) break;
  }
  writeAll(out);

  sent_.push_back(id);
  lastUsed_[id] = frame_;
  ids_[key] = id;
  pixelSize_[id] = {source->width, source->height};
  return id;
}

void KittyGraphics::forgetUnused() {
  constexpr uint64_t kUnusedFrames = 1800;
  std::string out;
  for (auto it = lastUsed_.begin(); it != lastUsed_.end();) {
    if (frame_ - it->second < kUnusedFrames) {
      ++it;
      continue;
    }
    const uint32_t id = it->first;
    out += "\x1b_Ga=d,d=I,q=2,i=" + std::to_string(id) + "\x1b\\";
    for (auto key = ids_.begin(); key != ids_.end();) key = key->second == id ? ids_.erase(key) : std::next(key);
    sent_.erase(std::remove(sent_.begin(), sent_.end(), id), sent_.end());
    pixelSize_.erase(id);
    it = lastUsed_.erase(it);
  }
  if (!out.empty()) writeAll(out);
}

void KittyGraphics::beginFrame() {
  ++frame_;
  if (frame_ % 300 == 0) forgetUnused();
  if (protocol_ != ImageProtocol::KittyDirect) return;
  placements_.clear();
  frameOpen_ = true;
}

void KittyGraphics::place(uint32_t id, int x, int y, int cellX0, int cellY0, int cellX1, int cellY1, int cols, int rows) {
  if (!frameOpen_ || cellX1 <= cellX0 || cellY1 <= cellY0 || cols <= 0 || rows <= 0) return;
  const auto size = pixelSize_.find(id);
  if (size == pixelSize_.end()) return;
  Placement p{id, x, y, cellX1 - cellX0, cellY1 - cellY0, 0, 0, 0, 0};
  if (cellX0 != 0 || cellY0 != 0 || cellX1 != cols || cellY1 != rows) {
    const auto [w, h] = size->second;
    p.srcX = cellX0 * w / cols;
    p.srcY = cellY0 * h / rows;
    p.srcW = std::max(1, cellX1 * w / cols - p.srcX);
    p.srcH = std::max(1, cellY1 * h / rows - p.srcY);
  }
  placements_.push_back(p);
}

void KittyGraphics::endFrame() {
  if (!frameOpen_) return;
  frameOpen_ = false;
  if (placements_ == shown_) {
    placements_.clear();
    return;
  }
  std::string out = "\x1b[?2026h";
  uint32_t shownId = 0;
  for (const Placement &old : shown_) out += "\x1b_Ga=d,d=i,q=2,i=" + std::to_string(old.id) + ",p=" + std::to_string(++shownId) + "\x1b\\";
  out += "\0337";
  uint32_t placementId = 0;
  for (const Placement &p : placements_) {
    out += "\x1b[" + std::to_string(p.y + 1) + ";" + std::to_string(p.x + 1) + "H";
    out += "\x1b_Ga=p,q=2,C=1,p=" + std::to_string(++placementId) + ",i=" + std::to_string(p.id) + ",c=" + std::to_string(p.cols) + ",r=" + std::to_string(p.rows);
    if (p.srcW > 0) out += ",x=" + std::to_string(p.srcX) + ",y=" + std::to_string(p.srcY) + ",w=" + std::to_string(p.srcW) + ",h=" + std::to_string(p.srcH);
    out += "\x1b\\";
  }
  out += "\0338";
  out += "\x1b[?2026l";
  // N8V_TUI_IMAGE_LOG=<file> records what each frame asks the terminal to drop and show, for chasing stale or doubled images.
  if (const char *logPath = std::getenv("N8V_TUI_IMAGE_LOG")) {
    if (FILE *f = std::fopen(logPath, "a")) {
      std::fprintf(f, "frame: drop %zu, show %zu\n", shown_.size(), placements_.size());
      for (const Placement &old : shown_) std::fprintf(f, "  drop id=%u at=(%d,%d) %dx%d\n", old.id, old.x, old.y, old.cols, old.rows);
      for (const Placement &p : placements_) std::fprintf(f, "  show id=%u at=(%d,%d) %dx%d crop=(%d,%d %dx%d)\n", p.id, p.x, p.y, p.cols, p.rows, p.srcX, p.srcY, p.srcW, p.srcH);
      std::fclose(f);
    }
  }
  writeAll(out);
  shown_ = std::move(placements_);
  placements_.clear();
}

void KittyGraphics::deleteAll() {
  std::string out;
  if (frameOpen_) {
    out += "\x1b[?2026l";
    frameOpen_ = false;
  }
  for (uint32_t id : sent_) out += "\x1b_Ga=d,d=I,q=2,i=" + std::to_string(id) + "\x1b\\";
  writeAll(out);
  sent_.clear();
  ids_.clear();
  lastUsed_.clear();
  shown_.clear();
}

} // namespace n8v::detail
