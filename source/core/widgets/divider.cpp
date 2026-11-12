#include <n8v/n8v_c.h>

#include "core/style.hpp"
#include "core/theme_color.hpp"
#include "core/tree/emit.hpp"
#include "core/ui_core_internal.hpp"

#include <clay.h>

namespace n8v::detail::emit {

void divider(const n8v_divider_options &options, int) {
  Clay__OpenElement();

  Clay_ElementDeclaration decl = {};
  decl.layout.sizing.width = CLAY_SIZING_GROW(0);
  decl.layout.sizing.height = CLAY_SIZING_FIXED(1);
  const n8v_color color = n8v::detail::resolveThemeColor(options.color);
  if (color.a > 0.0f) {
    decl.backgroundColor = {color.r, color.g, color.b, color.a};
  } else {
    const n8v::Color text = n8v::activePaint().text(n8v::TextOptions{}).color;
    decl.backgroundColor = {text.r, text.g, text.b, text.a * 0.3f};
  }

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

} // namespace n8v::detail::emit
