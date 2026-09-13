#include "bundled_font.hpp"

#include "bundle.hpp"
#include "bundle_n8v_fonts.h"

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
  static const bundle::archive archive(bundle_n8v_fonts_archive());
  auto entry = archive.find(fileNameFor(family, bold, italic));
  if (!entry) return {};
  std::vector<std::byte> bytes = archive.load(*entry);
  const auto *begin = reinterpret_cast<const unsigned char *>(bytes.data());
  return std::vector<unsigned char>(begin, begin + bytes.size());
}

} // namespace n8v::detail
