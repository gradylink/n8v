#include "bundled_font.hpp"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(n8v_fonts);

namespace n8v::detail {
namespace {

const char *fileNameFor(FontFamily family, bool bold, bool italic) {
  switch (family) {
  case FontFamily::Roboto:
    if (bold && italic) return "Roboto-BoldItalic.ttf";
    if (bold) return "Roboto-Bold.ttf";
    if (italic) return "Roboto-Italic.ttf";
    return "Roboto-Regular.ttf";
  case FontFamily::Inter:
    if (bold && italic) return "Inter-BoldItalic.ttf";
    if (bold) return "Inter-Bold.ttf";
    if (italic) return "Inter-Italic.ttf";
    return "Inter-Regular.ttf";
  case FontFamily::Selawik:
    return bold ? "Selawik-Bold.ttf" : "Selawik-Regular.ttf";
  case FontFamily::DejaVuSans:
  default:
    if (bold && italic) return "DejaVuSans-BoldOblique.ttf";
    if (bold) return "DejaVuSans-Bold.ttf";
    if (italic) return "DejaVuSans-Oblique.ttf";
    return "DejaVuSans.ttf";
  }
}

} // namespace

std::string bundledFontKey(FontFamily family, bool bold, bool italic) { return fileNameFor(family, bold, italic); }

std::vector<unsigned char> bundledFontBytes(FontFamily family, bool bold, bool italic) {
  static auto fs = cmrc::n8v_fonts::get_filesystem();
  auto file = fs.open(fileNameFor(family, bold, italic));
  return std::vector<unsigned char>(file.begin(), file.end());
}

} // namespace n8v::detail
