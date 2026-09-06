#include <iostream>
#include <n8v/backend.hpp>
#include <n8v/ui.hpp>
#include <ostream>

using namespace n8v;

int main() {
  Backend &backend = activeBackend();
  if (!backend.initialize(800, 600, "n8v basic example")) {
    return 1;
  }

  int clickCount = 0;

  while (backend.pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}}) {
        text({.bold = true})("n8v basic example");

        button({.style = ButtonStyle::Primary, .onClick = [&clickCount] {
                  ++clickCount;
                  std::cout << std::to_string(clickCount) << std::endl;
                }})("Click me");

        flex({.direction = Direction::Horizontal, .gap = 8}) {
          button({.style = ButtonStyle::Secondary})("Secondary");
          text({})("this text is a plain container, styled buttons above it, and a link below");
        }

        text({.italic = true, .url = "https://example.com"})("example.com");
      }
    }
  }

  backend.shutdown();
  return 0;
}
