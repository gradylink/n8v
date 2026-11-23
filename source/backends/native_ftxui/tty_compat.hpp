#pragma once

#include <cstddef>
#include <string>

#ifdef _WIN32
#include "core/platform/utf8_path.hpp"

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace n8v::detail::tty {

constexpr int kStdin = 0;
constexpr int kStdout = 1;
constexpr int kStderr = 2;

#ifdef _WIN32
inline bool isTty(int fd) { return _isatty(fd) != 0; }
inline int dupFd(int fd) { return _dup(fd); }
inline int dup2Fd(int from, int to) { return _dup2(from, to); }
inline void closeFd(int fd) { _close(fd); }
inline int openLogAppend(const std::string &path) { return _wopen(pathFromUtf8(path).c_str(), _O_WRONLY | _O_CREAT | _O_APPEND | _O_BINARY, _S_IREAD | _S_IWRITE); }
inline int openNullDevice() { return _open("NUL", _O_WRONLY | _O_BINARY); }
inline int makeDir(const std::string &path) { return _wmkdir(pathFromUtf8(path).c_str()); }

inline bool writeStdout(const char *data, size_t size) {
  size_t done = 0;
  while (done < size) {
    const size_t chunk = size - done > 0x10000 ? 0x10000 : size - done;
    const int n = _write(kStdout, data + done, static_cast<unsigned>(chunk));
    if (n <= 0) return false;
    done += static_cast<size_t>(n);
  }
  return true;
}
#else
inline bool isTty(int fd) { return isatty(fd) != 0; }
inline int dupFd(int fd) { return dup(fd); }
inline int dup2Fd(int from, int to) { return dup2(from, to); }
inline void closeFd(int fd) { close(fd); }
inline int openLogAppend(const std::string &path) { return open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600); }
inline int openNullDevice() { return open("/dev/null", O_WRONLY); }
inline int makeDir(const std::string &path) { return mkdir(path.c_str(), 0700); }

inline bool writeStdout(const char *data, size_t size) {
  size_t done = 0;
  while (done < size) {
    const ssize_t n = ::write(kStdout, data + done, size - done);
    if (n <= 0) return false;
    done += static_cast<size_t>(n);
  }
  return true;
}
#endif

} // namespace n8v::detail::tty
