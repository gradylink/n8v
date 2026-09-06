#include "bundled_font.hpp"

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(n8v_fonts);

namespace n8v::detail {
namespace {

const char *fileNameFor(bool bold, bool italic) {
  if (bold && italic) return "DejaVuSans-BoldOblique.ttf";
  if (bold) return "DejaVuSans-Bold.ttf";
  if (italic) return "DejaVuSans-Oblique.ttf";
  return "DejaVuSans.ttf";
}

} // namespace

std::string bundledFontKey(bool bold, bool italic) { return fileNameFor(bold, italic); }

std::vector<unsigned char> bundledFontBytes(bool bold, bool italic) {
  static auto fs = cmrc::n8v_fonts::get_filesystem();
  auto file = fs.open(fileNameFor(bold, italic));
  return std::vector<unsigned char>(file.begin(), file.end());
}

} // namespace n8v::detail
