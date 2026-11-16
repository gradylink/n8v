#include "clipboard_files.hpp"

#include "core/paste_file.hpp"

#include <SDL2/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace n8v::detail {

namespace {

std::filesystem::path pathFromUtf8(const std::string &text) { return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size())); }

bool isRegularFile(const std::string &path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(pathFromUtf8(path), ec);
}

std::vector<std::string> pathsFromUriText() {
  std::vector<std::string> paths;
  char *clip = SDL_GetClipboardText();
  if (!clip) return paths;
  std::istringstream lines(clip);
  SDL_free(clip);

  auto decode = [](const std::string &in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
      if (in[i] == '%' && i + 2 < in.size() && std::isxdigit((unsigned char)in[i + 1]) && std::isxdigit((unsigned char)in[i + 2])) {
        out += (char)std::strtol(in.substr(i + 1, 2).c_str(), nullptr, 16);
        i += 2;
      } else {
        out += in[i];
      }
    }
    return out;
  };

  for (std::string line; std::getline(lines, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("file://", 0) != 0) return {};
    std::string path = decode(line.substr(7));
#ifdef _WIN32
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') path.erase(0, 1); // file:///C:/dir/file
#endif
    if (isRegularFile(path)) paths.push_back(path);
  }
  return paths;
}

#if !defined(_WIN32)

std::string run(const std::string &command) {
  std::string out;
  if (FILE *pipe = popen(command.c_str(), "r")) {
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    pclose(pipe);
  }
  return out;
}

bool has(const std::string &types, const char *type) { return types.find(type) != std::string::npos; }

bool savedBy(const std::string &command, const std::string &path) {
  if (std::system(command.c_str()) != 0) return false;
  std::error_code ec;
  return std::filesystem::exists(path, ec) && std::filesystem::file_size(path, ec) > 0;
}

#endif

} // namespace

#if defined(_WIN32)

std::vector<std::string> clipboardFiles() {
  std::vector<std::string> paths = pathsFromUriText();
  if (!paths.empty()) return paths;
  if (!OpenClipboard(nullptr)) return paths;
  struct Closer {
    ~Closer() { CloseClipboard(); }
  } closer;

  auto utf8 = [](const std::wstring &wide) {
    if (wide.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), out.data(), size, nullptr, nullptr);
    return out;
  };

  if (HANDLE drop = GetClipboardData(CF_HDROP)) {
    HDROP files = (HDROP)drop;
    const UINT count = DragQueryFileW(files, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
      const UINT length = DragQueryFileW(files, i, nullptr, 0);
      std::wstring wide((size_t)length + 1, L'\0');
      DragQueryFileW(files, i, wide.data(), length + 1);
      wide.resize(length);
      std::error_code ec;
      if (std::filesystem::is_regular_file(std::filesystem::path(wide), ec)) paths.push_back(utf8(wide));
    }
    if (!paths.empty()) return paths;
  }

  if (IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT)) return paths;

  auto write = [&](const std::string &path, const void *head, size_t headSize, const void *data, size_t size) {
    std::ofstream file(pathFromUtf8(path), std::ios::binary);
    if (headSize) file.write(static_cast<const char *>(head), (std::streamsize)headSize);
    file.write(static_cast<const char *>(data), (std::streamsize)size);
    return (bool)file;
  };

  const UINT pngFormat = RegisterClipboardFormatA("PNG");
  if (pngFormat && IsClipboardFormatAvailable(pngFormat)) {
    if (HANDLE data = GetClipboardData(pngFormat)) {
      if (void *bytes = GlobalLock(data)) {
        const std::string path = newPastedImagePath("png");
        const bool ok = write(path, nullptr, 0, bytes, GlobalSize(data));
        GlobalUnlock(data);
        if (ok) paths.push_back(path);
        return paths;
      }
    }
  }

  if (IsClipboardFormatAvailable(CF_DIB)) {
    if (HANDLE data = GetClipboardData(CF_DIB)) {
      if (void *bytes = GlobalLock(data)) {
        const size_t size = GlobalSize(data);
        const auto *info = static_cast<const BITMAPINFOHEADER *>(bytes);
        size_t pixelsAt = info->biSize;
        if (info->biBitCount <= 8) pixelsAt += (info->biClrUsed ? info->biClrUsed : (1u << info->biBitCount)) * sizeof(RGBQUAD);
        if (info->biCompression == BI_BITFIELDS && info->biSize == sizeof(BITMAPINFOHEADER)) pixelsAt += 3 * sizeof(DWORD);
        BITMAPFILEHEADER header{};
        header.bfType = 0x4D42;
        header.bfSize = (DWORD)(sizeof(header) + size);
        header.bfOffBits = (DWORD)(sizeof(header) + pixelsAt);
        const std::string path = newPastedImagePath("bmp");
        const bool ok = write(path, &header, sizeof(header), bytes, size);
        GlobalUnlock(data);
        if (ok) paths.push_back(path);
      }
    }
  }
  return paths;
}

#elif defined(__APPLE__)

std::vector<std::string> clipboardFiles() {
  std::vector<std::string> paths = pathsFromUriText();
  if (!paths.empty()) return paths;

  const std::string info = run("osascript -e 'clipboard info' 2>/dev/null");
  if (info.empty()) return paths;

  if (has(info, "furl")) {
    std::string path = run("osascript -e 'POSIX path of (the clipboard as «class furl»)' 2>/dev/null");
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    if (!path.empty() && isRegularFile(path)) paths.push_back(path);
    if (!paths.empty()) return paths;
  }

  if (has(info, "utf8") || has(info, "ut16") || has(info, "TEXT")) return paths;

  struct Format {
    const char *code;
    const char *extension;
  };
  for (const Format &format : {Format{"PNGf", "png"}, Format{"JPEG", "jpg"}, Format{"GIFf", "gif"}, Format{"TIFF", "tiff"}}) {
    if (!has(info, format.code)) continue;
    const std::string path = newPastedImagePath(format.extension);
    const std::string command = std::string("osascript -e 'set f to open for access POSIX file \"") + path +
                                "\" with write permission' -e 'set eof f to 0' -e 'write (the clipboard as «class " + format.code +
                                "») to f' -e 'close access f' >/dev/null 2>&1";
    if (savedBy(command, path)) {
      paths.push_back(path);
      break;
    }
  }
  return paths;
}

#else

std::vector<std::string> clipboardFiles() {
  std::vector<std::string> paths = pathsFromUriText();
  if (!paths.empty()) return paths;

  const bool wayland = std::getenv("WAYLAND_DISPLAY") != nullptr;
  const std::string types = wayland ? run("wl-paste --list-types 2>/dev/null") : run("xclip -selection clipboard -t TARGETS -o 2>/dev/null");
  if (types.empty()) return paths;
  if (has(types, "text/plain") || has(types, "UTF8_STRING") || has(types, "STRING")) return paths;

  struct Format {
    const char *mime;
    const char *extension;
  };
  for (const Format &format :
       {Format{"image/png", "png"}, Format{"image/jpeg", "jpg"}, Format{"image/gif", "gif"}, Format{"image/webp", "webp"}, Format{"image/bmp", "bmp"}}) {
    if (!has(types, format.mime)) continue;
    const std::string path = newPastedImagePath(format.extension);
    const std::string command = wayland ? std::string("wl-paste --type ") + format.mime + " > '" + path + "' 2>/dev/null"
                                        : std::string("xclip -selection clipboard -t ") + format.mime + " -o > '" + path + "' 2>/dev/null";
    if (savedBy(command, path)) {
      paths.push_back(path);
      break;
    }
  }
  return paths;
}

#endif

} // namespace n8v::detail
