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
  bool compactSidebar = false;
  std::string name;
  int favoriteColor = 0;
  int favoriteFruit = -1;
  float volume = 0.5f;
  std::string potatoPath = N8V_EXAMPLE_ASSET_DIR "/potato.png";

  int selectedSection = 0;
  int selectedCategory = 0;

  while (pumpEvents()) {
    UI() {
      sidebar({.title = "Sections", .selected = &selectedSection, .width = Sizing::fixed(260), .compact = compactSidebar}) {
        page({.name = "Widgets", .icon = "home"}) {
          flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow(), .height = Sizing::grow(), .clipVertical = true}) {
            text({.bold = true})("n8v basic example");
            text({.strikethrough = true})("strikethrough example");

            panel({.role = PanelRole::Card, .gap = 4, .width = Sizing::grow()}) {
              text({.bold = true})("Card panel");
              panel({.role = PanelRole::ListItem, .width = Sizing::grow()}) { text("Row one"); }
              panel({.role = PanelRole::ListItem, .width = Sizing::grow()}) { text("Row two"); }
            }

            button({.style = ButtonStyle::Primary, .onClick = [&clickCount] {
                    ++clickCount;
                    std::cout << std::to_string(clickCount) << std::endl;
                  }, .icon = "check"})("Click me");

            flex({.direction = Direction::Horizontal, .gap = 8, .vAlign = Align::Center}) {
              button({.style = ButtonStyle::Ghost, .icon = "search"})("");
              button({.style = ButtonStyle::Ghost, .icon = "menu"})("");
              text("ghost icon buttons");
            }

            flex({.direction = Direction::Horizontal, .gap = 8, .vAlign = Align::Center}) {
              icon({.name = "settings"});
              icon({.name = "star", .tint = {230, 180, 20, 255}});
              icon({.name = "heart", .tint = {220, 40, 60, 255}});
              text("standalone icons");
            }

            checkbox({.checked = &passwordInput})("Password mode.");

            toggle({.checked = &compactSidebar})("Compact Sidebar");

            entry({.value = &name, .placeholder = passwordInput ? "Password" : "Your name", .password = passwordInput});

            flex({.direction = Direction::Vertical, .gap = 4}) {
              radio({.selected = &favoriteColor, .value = 0})("Red");
              radio({.selected = &favoriteColor, .value = 1})("Green");
              radio({.selected = &favoriteColor, .value = 2})("Blue");
            }

            dropdown({.items = {"Apple", "Banana", "Cherry"}, .selected = &favoriteFruit, .placeholder = "Pick a fruit"});

            slider({.value = &volume, .min = 0.0f, .max = 1.0f});

            image({
              .source = ImageSource::Path,
              .path = potatoPath,
              .width = Sizing::fixed(96),
              .height = Sizing::fixed(96),
              .rounding = Rounding::fixed({24, 24, 0, 0}),
            });

            flex({.direction = Direction::Vertical, .gap = 4, .width = Sizing::grow(), .height = Sizing::fixed(120), .clipVertical = true, .id = "scroll-container"}) {
              for (int i = 1; i <= 15; ++i) {
                flex({.id = "row-" + std::to_string(i)}) { text("Scrollable row " + std::to_string(i)); }
              }
            }
            button({.onClick = []() { scrollToBottom("scroll-container"); }})("Scroll to bottom.");
            button({.onClick = []() { scrollToElement("scroll-container", "row-8"); }})("Scroll to row 8.");

            flex({.direction = Direction::Horizontal, .gap = 8, .hAlign = Align::Center, .vAlign = Align::Center, .width = Sizing::grow()}) {
              button({.style = ButtonStyle::Secondary})("Secondary");
              text("this text is a plain container, styled buttons above it, and a link below");
            }

            text({.italic = true, .url = "https://example.com"})("example.com");
          }
        }

        page({.name = "Settings", .icon = "settings"}) {
          sidebar({.title = "Categories", .selected = &selectedCategory, .width = Sizing::fixed(160), .compact = true}) {
            page({.name = "Appearance"}) {
              flex({.direction = Direction::Vertical, .gap = 8, .padding = {20, 20, 20, 20}}) {
                text({.bold = true})("Appearance");
                text("Theme, colors, and font size go here.");
              }
            }
            page({.name = "Notifications"}) {
              flex({.direction = Direction::Vertical, .gap = 8, .padding = {20, 20, 20, 20}}) {
                text({.bold = true})("Notifications");
                text("Alert sounds and badges go here.");
              }
            }
            page({.name = "Privacy"}) {
              flex({.direction = Direction::Vertical, .gap = 8, .padding = {20, 20, 20, 20}}) {
                text({.bold = true})("Privacy");
                text("Data and permissions go here.");
              }
            }
          }
        }

        page({.name = "About", .icon = "info"}) {
          flex({.direction = Direction::Vertical, .gap = 12, .padding = {20, 20, 20, 20}, .width = Sizing::grow()}) {
            text({.bold = true})("About");
            text("n8v basic example.");
          }
        }
      }
    }
  }

  shutdown();
  return 0;
}
