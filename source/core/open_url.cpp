#include "open_url.hpp"

#include <n8v/n8v_c.h>

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace n8v::detail {

namespace {

std::function<bool(std::string_view)> &urlHandler() {
  static std::function<bool(std::string_view)> handler;
  return handler;
}

} // namespace

#if defined(_WIN32)

void openExternally(std::string_view url) {
  if (url.empty()) return;
  const int n = MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), nullptr, 0);
  if (n <= 0) return;
  std::wstring wide(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, url.data(), static_cast<int>(url.size()), wide.data(), n);
  ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#elif defined(__EMSCRIPTEN__)

void openExternally(std::string_view url) {
  std::string urlStr(url);
  EM_ASM({ window.open(UTF8ToString($0), '_blank'); }, urlStr.c_str());
}

#else

void openExternally(std::string_view url) {
  std::string urlStr(url);

  pid_t pid = fork();
  if (pid < 0) return;

  if (pid == 0) {
    if (fork() == 0) {
#if defined(__APPLE__)
      execlp("open", "open", urlStr.c_str(), (char *)nullptr);
#else
      execlp("xdg-open", "xdg-open", urlStr.c_str(), (char *)nullptr);
#endif
      _exit(127);
    }
    _exit(0);
  }

  int status;
  waitpid(pid, &status, 0);
}

#endif

void setUrlHandler(std::function<bool(std::string_view)> handler) { urlHandler() = std::move(handler); }

void openUrl(std::string_view url) {
  if (urlHandler() && urlHandler()(url)) return;
  openExternally(url);
}

} // namespace n8v::detail

extern "C" {

void n8v_set_url_handler(n8v_url_handler_fn handler, void *userdata) {
  if (!handler) {
    n8v::detail::setUrlHandler(nullptr);
    return;
  }
  n8v::detail::setUrlHandler([handler, userdata](std::string_view url) {
    const std::string urlStorage(url);
    return handler(urlStorage.c_str(), userdata);
  });
}

} // extern "C"
