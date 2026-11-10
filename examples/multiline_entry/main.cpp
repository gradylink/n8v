#include <n8v/ui.hpp>

#include <cstdio>
#include <string>

using namespace n8v;

namespace {

void report(const char *name, const std::string &value) {
  std::string shown;
  for (char c : value) shown += c == '\n' ? std::string("\\n") : std::string(1, c);
  std::printf("%s [%s]\n", name, shown.c_str());
  std::fflush(stdout);
}

} // namespace

int main() {
  if (!initialize(520, 560, "n8v multiline entries")) {
    return 1;
  }

  std::string notes, message, single;
  int notesSubmits = 0, messageSubmits = 0, singleSubmits = 0;

  while (pumpEvents()) {
    UI() {
      flex({.direction = Direction::Vertical, .gap = 10, .padding = {16, 16, 16, 16}, .width = Sizing::grow(), .height = Sizing::grow()}) {
        text({.bold = true})("Fixed height, Ctrl+Enter submits (Enter inserts a newline)");
        entry({.value = &notes,
               .placeholder = "Notes",
               .onSubmit = [&] { ++notesSubmits; report("notes", notes); },
               .id = "notes",
               .multiline = true,
               .submitMode = SubmitMode::CtrlEnter,
               .minLines = 3});

        text({.bold = true})("Grows from 1 to 5 lines, Enter submits (Shift+Enter inserts a newline), styled like the single-line entry");
        entry({.value = &message,
               .placeholder = "Message",
               .onSubmit = [&] { ++messageSubmits; report("message", message); },
               .id = "message",
               .multiline = true,
               .submitMode = SubmitMode::Enter,
               .minLines = 1,
               .maxLines = 5,
               .matchEntryStyle = true});

        text({.bold = true})("Single line");
        entry({.value = &single, .placeholder = "Single line", .onSubmit = [&] { ++singleSubmits; report("single", single); }, .id = "single"});

        text("submits: notes=" + std::to_string(notesSubmits) + " message=" + std::to_string(messageSubmits) + " single=" + std::to_string(singleSubmits));
        text("lengths: notes=" + std::to_string(notes.size()) + " message=" + std::to_string(message.size()) + " single=" + std::to_string(single.size()));
      }
    }
  }

  shutdown();
  return 0;
}
