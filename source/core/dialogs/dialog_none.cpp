#include "file_dialog.hpp"

namespace n8v::detail {

bool startPlatformFileDialog(const FileDialogRequest &, FileDialogDone) { return false; }

void pollPlatformFileDialog() {}

} // namespace n8v::detail
