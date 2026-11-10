#include "file_dialog.hpp"

#include "core/backend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>

namespace n8v::detail {

std::string normalizeFileGlob(std::string_view pattern) {
  if (pattern == "*" || pattern == "*.*" || pattern.empty()) return "*";
  if (pattern.front() == '*') return std::string(pattern);
  if (pattern.front() == '.') return "*" + std::string(pattern);
  return "*." + std::string(pattern);
}

std::string globExtension(std::string_view glob) {
  if (glob.size() < 3 || glob.substr(0, 2) != "*.") return {};
  std::string_view ext = glob.substr(2);
  if (ext.find_first_of("*?[]") != std::string_view::npos) return {};
  return std::string(ext);
}

std::string joinFilters(const std::vector<FileDialogFilter> &filters) {
  std::string out;
  for (const FileDialogFilter &f : filters) {
    if (!out.empty()) out += ";;";
    std::string globs;
    for (const std::string &p : f.patterns) {
      if (!globs.empty()) globs += ' ';
      globs += p;
    }
    out += f.name + " (" + globs + ")";
  }
  return out;
}

std::string fileUriToPath(std::string_view uri) {
  constexpr std::string_view scheme = "file://";
  if (uri.substr(0, scheme.size()) != scheme) return {};
  uri.remove_prefix(scheme.size());
  if (!uri.empty() && uri.front() != '/') {
    size_t slash = uri.find('/');
    if (slash == std::string_view::npos || uri.substr(0, slash) != "localhost") return {};
    uri.remove_prefix(slash);
  }
  auto hex = [](char c) { return std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : std::tolower(static_cast<unsigned char>(c)) - 'a' + 10; };
  std::string out;
  out.reserve(uri.size());
  for (size_t i = 0; i < uri.size(); ++i) {
    if (uri[i] == '%' && i + 2 < uri.size() && std::isxdigit(static_cast<unsigned char>(uri[i + 1])) && std::isxdigit(static_cast<unsigned char>(uri[i + 2]))) {
      out += static_cast<char>(hex(uri[i + 1]) * 16 + hex(uri[i + 2]));
      i += 2;
    } else {
      out += uri[i];
    }
  }
  return out;
}

namespace {

enum class Policy { Both, ToolkitOnly, SystemOnly };

Policy policyFromEnv() {
  const char *env = std::getenv("N8V_FILE_DIALOG");
  if (!env) return Policy::Both;
  if (std::strcmp(env, "toolkit") == 0) return Policy::ToolkitOnly;
  if (std::strcmp(env, "system") == 0) return Policy::SystemOnly;
  return Policy::Both;
}

struct Job {
  FileDialogRequest request;
  FileDialogDone done;
};

struct Finished {
  FileDialogResult result;
  FileDialogDone done;
};

class Dispatcher {
public:
  void enqueue(FileDialogRequest request, FileDialogDone done) { waiting_.push_back({std::move(request), std::move(done)}); }

  void pump() {
    if (!active_ && !waiting_.empty()) startNext();
    if (active_) pollPlatformFileDialog();
    deliver();
    if (!active_ && !waiting_.empty()) {
      startNext();
      deliver();
    }
  }

private:
  enum class Stage { Platform, Toolkit };

  void startNext() {
    active_ = std::make_unique<Job>(std::move(waiting_.front()));
    waiting_.pop_front();
    policy_ = policyFromEnv();
    ++generation_;
    stage_ = Stage::Platform;
    tryStage();
  }

  void tryStage() {
    const unsigned gen = generation_;
    auto done = [this, gen](FileDialogResult result) {
      if (gen != generation_ || !active_) return;
      onResult(std::move(result));
    };

    if (stage_ == Stage::Platform) {
      if (policy_ != Policy::ToolkitOnly && startPlatformFileDialog(active_->request, done)) return;
      stage_ = Stage::Toolkit;
    }
    if (policy_ != Policy::SystemOnly && activeBackend().showFileDialog(active_->request, done)) return;

    std::fprintf(stderr, "[n8v] No file dialog is available on this system.\n");
    finish({FileDialogResult::Status::Cancelled, {}});
  }

  void onResult(FileDialogResult result) {
    if (result.status == FileDialogResult::Status::Unavailable) {
      if (stage_ == Stage::Platform) {
        stage_ = Stage::Toolkit;
        ++generation_;
        tryStage();
      } else {
        std::fprintf(stderr, "[n8v] No file dialog is available on this system.\n");
        finish({FileDialogResult::Status::Cancelled, {}});
      }
      return;
    }
    finish(std::move(result));
  }

  void finish(FileDialogResult result) {
    if (result.status != FileDialogResult::Status::Done) result.paths.clear();
    finished_.push_back({std::move(result), std::move(active_->done)});
    active_.reset();
    ++generation_;
  }

  void deliver() {
    while (!finished_.empty()) {
      Finished f = std::move(finished_.front());
      finished_.pop_front();
      if (f.done) f.done(std::move(f.result));
    }
  }

  std::deque<Job> waiting_;
  std::deque<Finished> finished_;
  std::unique_ptr<Job> active_;
  Stage stage_ = Stage::Platform;
  Policy policy_ = Policy::Both;
  unsigned generation_ = 0;
};

Dispatcher &dispatcher() {
  static Dispatcher d;
  return d;
}

} // namespace

void showFileDialog(FileDialogRequest request, FileDialogDone done) { dispatcher().enqueue(std::move(request), std::move(done)); }

void pumpFileDialogs() { dispatcher().pump(); }

} // namespace n8v::detail
