#include <n8v/n8v_c.h>

#include "core/tree/emit.hpp"

namespace n8v::detail::emit {

void video(const n8v_video_options &options, int ordinal) {
  n8v_text_options link{};
  link.url = options.source;
  text(link, "\xE2\x96\xB6 Play video", ordinal);
}

void audio(const n8v_audio_options &options, int ordinal) {
  n8v_text_options link{};
  link.url = options.source;
  text(link, "\xE2\x99\xAB Play audio", ordinal);
}

} // namespace n8v::detail::emit
