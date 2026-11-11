#include <n8v/ui.hpp>

#include <string>

using namespace n8v;

int main() {
  if (!initialize(640, 360, "n8v toggle buttons")) {
    return 1;
  }

  bool bold = false;
  bool italic = true;
  bool ping = true;
  bool muted = false;
  std::string status = "Click a toggle.";

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow(), .height = Sizing::grow()}) {
        text({.bold = true})("Toggle buttons");
        text(status);

        flex({.gap = 8}) {
          toggleButton({.checked = &bold, .onChange = [&](bool on) { status = std::string("bold: ") + (on ? "on" : "off"); }})("Bold");
          toggleButton({.checked = &italic, .onChange = [&](bool on) { status = std::string("italic: ") + (on ? "on" : "off"); }, .style = ButtonStyle::Primary})("Italic");
          toggleButton({.checked = &ping, .onChange = [&](bool on) { status = std::string("ping: ") + (on ? "on" : "off"); }, .style = ButtonStyle::Ghost, .icon = ping ? "notification" : "notification-off"})("Ping");
          toggleButton({.checked = &muted, .onChange = [&](bool on) { status = std::string("muted: ") + (on ? "on" : "off"); }, .style = ButtonStyle::Ghost, .icon = muted ? "notification-off" : "notification"})("");
        }
      }
    }
  }

  shutdown();
  return 0;
}
