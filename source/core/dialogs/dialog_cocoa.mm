#include "file_dialog.hpp"

#import <AppKit/AppKit.h>

#include <string>
#include <vector>

namespace n8v::detail {
namespace {

NSString *ns(const std::string &s) { return [NSString stringWithUTF8String:s.c_str()]; }

NSArray<NSString *> *allowedExtensions(const std::vector<FileDialogFilter> &filters) {
  NSMutableArray<NSString *> *exts = [NSMutableArray array];
  for (const FileDialogFilter &f : filters) {
    for (const std::string &p : f.patterns) {
      std::string ext = globExtension(p);
      if (ext.empty()) return nil;
      [exts addObject:ns(ext)];
    }
  }
  return exts.count ? exts : nil;
}

std::vector<std::string> run(const FileDialogRequest &request) {
  const bool save = request.kind == N8V_FILE_DIALOG_SAVE_FILE;
  NSSavePanel *panel = save ? [NSSavePanel savePanel] : [NSOpenPanel openPanel];

  if (!request.title.empty()) panel.message = ns(request.title);
  if (!request.initialDir.empty()) panel.directoryURL = [NSURL fileURLWithPath:ns(request.initialDir) isDirectory:YES];
  if (save) {
    if (!request.defaultName.empty()) panel.nameFieldStringValue = ns(request.defaultName);
    panel.canCreateDirectories = YES;
  } else {
    NSOpenPanel *open = (NSOpenPanel *)panel;
    const bool folders = request.kind == N8V_FILE_DIALOG_OPEN_FOLDER;
    open.canChooseFiles = !folders;
    open.canChooseDirectories = folders;
    open.canCreateDirectories = folders;
    open.allowsMultipleSelection = request.multiple;
  }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if (request.kind != N8V_FILE_DIALOG_OPEN_FOLDER) panel.allowedFileTypes = allowedExtensions(request.filters);
#pragma clang diagnostic pop

  std::vector<std::string> paths;
  if ([panel runModal] != NSModalResponseOK) return paths;
  if (save) {
    if (panel.URL.path) paths.emplace_back(panel.URL.path.UTF8String);
  } else {
    for (NSURL *url in ((NSOpenPanel *)panel).URLs) {
      if (url.path) paths.emplace_back(url.path.UTF8String);
    }
  }
  return paths;
}

} // namespace

bool startPlatformFileDialog(const FileDialogRequest &request, FileDialogDone done) {
  std::vector<std::string> paths;
  @autoreleasepool {
    [NSApplication sharedApplication];
    paths = run(request);
  }
  done({paths.empty() ? FileDialogResult::Status::Cancelled : FileDialogResult::Status::Done, std::move(paths)});
  return true;
}

void pollPlatformFileDialog() {}

} // namespace n8v::detail
