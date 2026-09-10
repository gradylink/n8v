#include "n8v/types.hpp"
#include <iostream>
#include <n8v/ui.hpp>
#include <ostream>
#include <string>

using namespace n8v;

int main() {
  if (!initialize(800, 600, "n8v basic example")) {
    return 1;
  }

  int clickCount = 0;
  bool passwordInput = false;
  std::string name;
  int favoriteColor = 0;
  int favoriteFruit = -1;
  float volume = 0.5f;

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow()}) {
        text({.bold = true})("n8v basic example");

        button({.style = ButtonStyle::Primary, .onClick = [&clickCount] {
                  ++clickCount;
                  std::cout << std::to_string(clickCount) << std::endl;
                }})("Click me");

        checkbox({.checked = &passwordInput})("Password mode.");

        entry({.value = &name, .placeholder = passwordInput ? "Password" : "Your name", .password = passwordInput});

        flex({.direction = Direction::Vertical, .gap = 4}) {
          radio({.selected = &favoriteColor, .value = 0})("Red");
          radio({.selected = &favoriteColor, .value = 1})("Green");
          radio({.selected = &favoriteColor, .value = 2})("Blue");
        }

        dropdown({.items = {"Apple", "Banana", "Cherry"}, .selected = &favoriteFruit, .placeholder = "Pick a fruit"});

        slider({.value = &volume, .min = 0.0f, .max = 1.0f});

        flex({.direction = Direction::Horizontal, .gap = 8, .hAlign = Align::Center, .vAlign = Align::Center, .width = Sizing::grow()}) {
          button({.style = ButtonStyle::Secondary})("Secondary");
          text({})("this text is a plain container, styled buttons above it, and a link below");
        }

        text({.italic = true, .url = "https://example.com"})("example.com");
      }
    }
  }

  shutdown();
  return 0;
}
