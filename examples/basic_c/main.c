#define N8V_NO_PREFIX
#include <n8v/n8v_c.h>

#include <stdbool.h>
#include <stdio.h>

static int click_count = 0;
static bool password_input = false;
static bool dark_mode = false;
static int favorite_color = 0;
static int favorite_fruit = -1;
static float volume = 0.5f;
static const char *potato_path = N8V_EXAMPLE_ASSET_DIR "/potato.png";

static void on_click(void *userdata) {
  (void)userdata;
  ++click_count;
  printf("%d\n", click_count);
}

static void on_password_toggle(bool value, void *userdata) {
  (void)userdata;
  password_input = value;
}

int main(void) {
  if (!n8v_initialize(800, 600, "n8v basic C example")) {
    return 1;
  }

  string_buf name;
  string_buf_init(&name);

  while (n8v_pump_events()) {
    UI() {
      flex(((flex_options){
        .direction = N8V_DIRECTION_VERTICAL,
        .gap = 12,
        .padding = {20, 20, 20, 20},
        .width = sizing_grow(0, 0),
      })) {
        text(((text_options){.bold = true}))("n8v basic C example");

        button(((button_options){
          .style = N8V_BUTTON_STYLE_PRIMARY,
          .on_click = on_click,
          .icon = "check",
        }))("Click me");

        flex(((flex_options){.direction = N8V_DIRECTION_HORIZONTAL, .gap = 8, .v_align = N8V_ALIGN_CENTER})) {
          n8v_icon((icon_options){.name = "settings"});
          n8v_icon((icon_options){.name = "star", .tint = {230, 180, 20, 255}});
          n8v_icon((icon_options){.name = "heart", .tint = {220, 40, 60, 255}});
          text("standalone icons");
        }

        checkbox(((checkbox_options){
          .checked = &password_input,
          .on_change = on_password_toggle,
        }))("Password mode.");

        toggle(((toggle_options){.checked = &dark_mode}))("Dark mode");

        entry((entry_options){
          .value = &name,
          .placeholder = password_input ? "Password" : "Your name",
          .password = password_input,
        });

        flex(((flex_options){.direction = N8V_DIRECTION_VERTICAL, .gap = 4})) {
          radio(((radio_options){.selected = &favorite_color, .value = 0}))("Red");
          radio(((radio_options){.selected = &favorite_color, .value = 1}))("Green");
          radio(((radio_options){.selected = &favorite_color, .value = 2}))("Blue");
        }

        static const char *fruit_items[] = {"Apple", "Banana", "Cherry"};
        dropdown((dropdown_options){
          .items = fruit_items,
          .item_count = 3,
          .selected = &favorite_fruit,
          .placeholder = "Pick a fruit",
        });

        slider((slider_options){.value = &volume, .min = 0.0f, .max = 1.0f});

        image((image_options){
          .source_kind = N8V_IMAGE_SOURCE_PATH,
          .path = potato_path,
          .width = sizing_fixed(96),
          .height = sizing_fixed(96),
          .rounding = rounding_fixed(48, 48, 0, 0),
        });

        flex(((flex_options){
          .direction = N8V_DIRECTION_HORIZONTAL,
          .gap = 8,
          .h_align = N8V_ALIGN_CENTER,
          .v_align = N8V_ALIGN_CENTER,
          .width = sizing_grow(0, 0),
        })) {
          button(((button_options){.style = N8V_BUTTON_STYLE_SECONDARY}))("Secondary");
          text("this text is a plain container, styled buttons above it, and a link below");
        }

        text(((text_options){.italic = true, .url = "https://example.com"}))("example.com");
      }
    }
  }

  string_buf_free(&name);
  n8v_shutdown();
  return 0;
}
