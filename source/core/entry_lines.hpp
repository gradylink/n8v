#pragma once

#include <n8v/n8v_c.h>

namespace n8v::detail {

struct EntryLines {
  int min;
  int max;
  bool fixed;
};

inline EntryLines entryLines(const n8v_entry_options &o) {
  const int min = o.min_lines ? o.min_lines : 3;
  const bool fixed = o.max_lines <= min;
  return {min, fixed ? min : static_cast<int>(o.max_lines), fixed};
}

inline bool enterSubmits(n8v_submit_mode mode, bool ctrl, bool shift) { return mode == N8V_SUBMIT_ON_CTRL_ENTER ? ctrl : !shift; }

} // namespace n8v::detail
