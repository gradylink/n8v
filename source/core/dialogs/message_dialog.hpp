#pragma once

#include <n8v/n8v_c.h>

#include <functional>
#include <string>
#include <vector>

namespace n8v::detail {

struct MessageDialogButton {
  std::string label;
  bool destructive = false;
};

struct MessageDialogRequest {
  std::string title;
  std::string message;
  n8v_message_dialog_kind kind = N8V_MESSAGE_DIALOG_INFO;
  std::vector<MessageDialogButton> buttons;
};

using MessageDialogDone = std::function<void(int button)>;

void showMessageDialog(MessageDialogRequest request, MessageDialogDone done);

void pumpMessageDialogs();

} // namespace n8v::detail
