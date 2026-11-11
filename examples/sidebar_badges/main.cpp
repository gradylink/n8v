#include <n8v/ui.hpp>

using namespace n8v;

int main() {
  if (!initialize(720, 480, "n8v sidebar badges")) {
    return 1;
  }

  int server = 1;
  int channel = 0;

  while (pumpEvents()) {
    UI() {
      sidebar({.title = "Servers", .selected = &server}) {
        page({.name = "Quiet server"}) { text("Nothing new."); }
        page({.name = "Busy server", .notify = true, .badge = "3", .badgeColor = BadgeColor::Red}) {
          sidebar({.title = "Channels", .selected = &channel, .width = Sizing::fixed(300), .compact = true}) {
            page({.name = "general", .notify = true, .badge = "3", .badgeColor = BadgeColor::Red}) { text("general"); }
            page({.name = "devs-chat", .notify = true}) { text("devs-chat"); }
            page({.name = "support", .badge = "new", .badgeColor = BadgeColor::Blue}) { text("support"); }
            page({.name = "off-topic", .badge = "12", .badgeColor = BadgeColor::Green}) { text("off-topic"); }
            page({.name = "plain"}) { text("plain"); }
          }
        }
        page({.name = "Colors", .badge = "99+", .badgeColor = BadgeColor::Orange}) {
          flex({.direction = Direction::Vertical, .gap = 8, .padding = {20, 20, 20, 20}}) { text("Pick another page."); }
        }
      }
    }
  }

  shutdown();
  return 0;
}
