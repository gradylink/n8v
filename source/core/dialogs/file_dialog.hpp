#pragma once

#include <n8v/n8v_c.h>

#include <functional>
#include <string>
#include <vector>

namespace n8v::detail {

struct FileDialogFilter {
  std::string name;
  std::vector<std::string> patterns;
};

struct FileDialogRequest {
  n8v_file_dialog_kind kind = N8V_FILE_DIALOG_OPEN_FILE;
  std::string title;
  std::string initialDir;
  std::string defaultName;
  std::vector<FileDialogFilter> filters;
  bool multiple = false;
};

struct FileDialogResult {
  enum class Status {
    Done,
    Cancelled,
    Unavailable,
  };
  Status status = Status::Cancelled;
  std::vector<std::string> paths;
};

using FileDialogDone = std::function<void(FileDialogResult)>;

std::string normalizeFileGlob(std::string_view pattern);

std::string globExtension(std::string_view glob);

std::string joinFilters(const std::vector<FileDialogFilter> &filters);

std::string fileUriToPath(std::string_view uri);

void showFileDialog(FileDialogRequest request, FileDialogDone done);

void pumpFileDialogs();

bool startPlatformFileDialog(const FileDialogRequest &request, FileDialogDone done);
void pollPlatformFileDialog();

} // namespace n8v::detail
