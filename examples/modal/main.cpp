#include <n8v/ui.hpp>
#include <cstdio>
using namespace n8v;
int main() {
  if (!initialize(760, 480, "n8v modal and sidebar buttons")) return 1;
  int server = 0, under = 0, inside = 0;
  bool open = false, flag = false;
  while (pumpEvents()) {
    UI() {
      sidebar({.title = "Servers", .selected = &server, .leftButton = SidebarButton{.icon = "settings", .tooltip = "Settings", .onClick = [&] { open = true; }}}) {
        page({.name = "One"}) {
          flex({.direction = Direction::Vertical, .gap = 8, .padding = {16, 16, 16, 16}}) {
            button({.onClick = [&] { ++under; }})("Underneath: " + std::to_string(under));
            text("modal open: " + std::string(open ? "yes" : "no"));
          }
        }
        page({.name = "Two"}) { text("two"); }
      }
      if (open) {
        modal({.title = "Server settings", .width = Sizing::fixed(420), .onClose = [&] { open = false; }}) {
          text("Some settings live here.");
          checkbox({.checked = &flag})("A setting");
          button({.onClick = [&] { ++inside; }})("Inside: " + std::to_string(inside));
        }
      }
    }
  }
  shutdown();
}
