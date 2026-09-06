#include "font_resolve.hpp"

#include <array>
#include <sys/stat.h>

namespace n8v::detail {
namespace {

bool fileExists(const char *path) {
  struct stat st{};
  return ::stat(path, &st) == 0;
}

} // namespace

std::string resolveSystemFont(bool bold, bool italic) {
  struct Candidate {
    const char *regular;
    const char *bold;
    const char *italic;
    const char *boldItalic;
  };
  static constexpr std::array<Candidate, 2> families{{
    {"/usr/share/fonts/TTF/DejaVuSans.ttf",
     "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
     "/usr/share/fonts/TTF/DejaVuSans-Oblique.ttf",
     "/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf",
     "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf"},
  }};

  for (const Candidate &family : families) {
    const char *chosen = bold && italic ? family.boldItalic : bold ? family.bold : italic ? family.italic : family.regular;
    if (fileExists(chosen)) return chosen;
  }

  return {};
}

} // namespace n8v::detail
