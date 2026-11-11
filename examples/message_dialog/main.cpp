#include <n8v/ui.hpp>

#include <string>

using namespace n8v;

int main() {
  if (!initialize(640, 480, "n8v message dialogs")) {
    return 1;
  }

  std::string status = "Pick a dialog.";

  auto report = [&](const char *what) { return [&, what](int button) { status = std::string(what) + ": " + (button < 0 ? "dismissed" : "button " + std::to_string(button)); }; };

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow(), .height = Sizing::grow()}) {
        text({.bold = true})("Message dialogs");
        text(status);

        flex({.gap = 8}) {
          button({.onClick = [&] { showMessageDialog({.title = "Heads up", .message = "Something happened."}, report("info")); }})("Info");
          button({.onClick = [&] { showMessageDialog({.title = "Careful", .message = "This might be a problem.", .kind = MessageKind::Warning}, report("warning")); }})("Warning");
          button({.onClick = [&] { showMessageDialog({.title = "Upload failed", .message = "The file is too large.", .kind = MessageKind::Error}, report("error")); }})("Error");
          button({.onClick = [&] { showMessageDialog({.title = "Delete?", .message = "This can't be undone.", .kind = MessageKind::Warning, .buttons = {{"Delete", true}, "Cancel"}}, report("choice")); }})(
            "Choice"
          );
        }
      }
    }
  }

  shutdown();
  return 0;
}
