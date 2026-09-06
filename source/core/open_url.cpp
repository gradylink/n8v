#include "open_url.hpp"

#include <string>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace n8v::detail {

#if defined(_WIN32)

void openUrl(std::string_view url) {
  std::string urlStr(url);
  ShellExecuteA(nullptr, "open", urlStr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

#else

void openUrl(std::string_view url) {
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

} // namespace n8v::detail
