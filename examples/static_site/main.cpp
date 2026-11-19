#include <n8v/ui.hpp>

#include <string>

using namespace n8v;

int main() {
  if (!initialize(900, 640, "n8v static site")) return 1;

  bool newsletter = true;
  int plan = 1;
  int favorite = 0;
  float volume = 0.4f;
  std::string email;
  std::string message = "Hello!\nThis page was rendered ahead of time.";

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 16, .padding = {32, 32, 24, 24}, .width = Sizing::grow(), .height = Sizing::grow(), .clipVertical = true}) {
        text({.large = true, .bold = true})("Static Example");
        text({.darken = true})("n8v can output static html now.");

        panel({.role = PanelRole::Card, .gap = 8, .width = Sizing::grow()}) {
          text({.bold = true})("Features");
          panel({.role = PanelRole::ListItem, .width = Sizing::grow()}) { text("it works"); }
          panel({.role = PanelRole::ListItem, .width = Sizing::grow()}) { text("it's not wxWidgets"); }
          panel({.role = PanelRole::ListItem, .width = Sizing::grow()}) { text("feature 3 :P"); }
        }

        flex({.direction = Direction::Horizontal, .gap = 8, .vAlign = Align::Center}) {
          button({.style = ButtonStyle::Primary, .onClick = [] {}})("Get started");
          button({.style = ButtonStyle::Secondary})("Documentation");
          text({.url = "https://example.com"})("example.com");
        }

        panel({.role = PanelRole::Card, .gap = 10, .width = Sizing::grow()}) {
          text({.bold = true})("Stay in touch");
          entry({.value = &email, .placeholder = "you@example.com"});
          entry({.value = &message, .multiline = true, .minLines = 3});
          checkbox({.checked = &newsletter})("Send me the newsletter");
          flex({.direction = Direction::Vertical, .gap = 4}) {
            radio({.selected = &plan, .value = 0})("Free");
            radio({.selected = &plan, .value = 1})("Pro");
            radio({.selected = &plan, .value = 2})("Team");
          }
          dropdown({.items = {"Apple", "Banana", "Cherry"}, .selected = &favorite, .placeholder = "Favorite fruit"});
          slider({.value = &volume, .min = 0.0f, .max = 1.0f});
        }
      }
    }
  }
  return 0;
}
