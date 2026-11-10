#include <n8v/ui.hpp>

#include <cstdio>
#include <string>
#include <vector>

using namespace n8v;

int main() {
  if (!initialize(640, 480, "n8v file dialogs")) {
    return 1;
  }

  std::string status = "Pick something.";
  std::vector<std::string> picked;

  auto report = [&](const char *what) {
    return [&, what](const std::vector<std::string> &paths) {
      picked = paths;
      status = paths.empty() ? std::string(what) + ": cancelled" : std::string(what) + ": " + std::to_string(paths.size()) + " selected";
      for (const std::string &p : paths) std::printf("%s: %s\n", what, p.c_str());
      std::fflush(stdout);
    };
  };

  const std::vector<FileFilter> images = {{"Images", {"png", ".jpg", "*.jpeg", "gif"}}, {"All files", {"*"}}};

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow(), .height = Sizing::grow()}) {
        text({.bold = true})("File dialogs");
        text(status);

        flex({.gap = 8}) {
          button({.onClick = [&] { openFileDialog({.title = "Open an image", .filters = images}, report("open")); }})("Open file");
          button({.onClick = [&] { openFileDialog({.title = "Open images", .filters = images, .multiple = true}, report("open multiple")); }})("Open several");
          button({.onClick = [&] { openFolderDialog({.title = "Choose a folder"}, report("folder")); }})("Choose folder");
          button({.onClick = [&] { saveFileDialog({.title = "Save as", .defaultName = "untitled.png", .filters = images}, report("save")); }})("Save as...");
        }

        flex({.direction = Direction::Vertical, .gap = 4, .width = Sizing::grow(), .height = Sizing::grow(), .clipVertical = true, .id = "paths"}) {
          for (const std::string &p : picked) text(p);
        }
      }
    }
  }

  shutdown();
  return 0;
}
