#include "message_dialog.hpp"

#include "core/backend.hpp"

#include <cstdio>
#include <deque>
#include <memory>

namespace n8v::detail {
namespace {

struct Job {
  MessageDialogRequest request;
  MessageDialogDone done;
};

struct Finished {
  int button;
  MessageDialogDone done;
};

class Dispatcher {
public:
  void enqueue(Job job) { waiting_.push_back(std::move(job)); }

  void pump() {
    if (!active_ && !waiting_.empty()) startNext();
    deliver();
  }

private:
  void startNext() {
    active_ = std::make_unique<Job>(std::move(waiting_.front()));
    waiting_.pop_front();
    const unsigned gen = ++generation_;
    auto done = [this, gen](int button) {
      if (gen != generation_ || !active_) return;
      finish(button);
    };
    if (activeBackend().showMessageDialog(active_->request, done)) return;

    std::fprintf(stderr, "[n8v] No message dialog is available: %s\n", active_->request.message.c_str());
    finish(-1);
  }

  void finish(int button) {
    finished_.push_back({button, std::move(active_->done)});
    active_.reset();
    ++generation_;
  }

  void deliver() {
    while (!finished_.empty()) {
      Finished f = std::move(finished_.front());
      finished_.pop_front();
      if (f.done) f.done(f.button);
    }
  }

  std::deque<Job> waiting_;
  std::deque<Finished> finished_;
  std::unique_ptr<Job> active_;
  unsigned generation_ = 0;
};

Dispatcher &dispatcher() {
  static Dispatcher d;
  return d;
}

} // namespace

void showMessageDialog(MessageDialogRequest request, MessageDialogDone done) {
  if (request.buttons.empty()) request.buttons.push_back({"OK", false});
  dispatcher().enqueue({std::move(request), std::move(done)});
}

void pumpMessageDialogs() { dispatcher().pump(); }

} // namespace n8v::detail
