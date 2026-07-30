//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/KeyboardButtonStyle.h"
#include "td/telegram/RequestedDialogType.h"
#include "td/telegram/td_api.h"
#include "td/telegram/telegram_api.h"

#include "td/utils/common.h"
#include "td/utils/Status.h"
#include "td/utils/StringBuilder.h"

namespace td {

struct KeyboardButton {
  // append only
  enum class Type : int32 {
    Text,
    RequestPhoneNumber,
    RequestLocation,
    RequestPoll,
    RequestPollQuiz,
    RequestPollRegular,
    WebView,
    RequestDialog
  };
  Type type_ = Type::Text;
  KeyboardButtonStyle style_;
  string text_;
  string url_;                                             // WebView only
  unique_ptr<RequestedDialogType> requested_dialog_type_;  // RequestDialog only

  KeyboardButton() = default;

  explicit KeyboardButton(telegram_api::object_ptr<telegram_api::keyboardButton> &&keyboard_button);

  static Result<KeyboardButton> get_keyboard_button(td_api::object_ptr<td_api::keyboardButton> &&button,
                                                    bool request_buttons_allowed);

  KeyboardButton clone() const;

  telegram_api::object_ptr<telegram_api::keyboardButton> get_input_keyboard_button() const;

  td_api::object_ptr<td_api::keyboardButton> get_keyboard_button_object() const;

  const RequestedDialogType *get_requested_dialog_type() const {
    return requested_dialog_type_.get();
  }
};

bool operator==(const KeyboardButton &lhs, const KeyboardButton &rhs);

StringBuilder &operator<<(StringBuilder &string_builder, const KeyboardButton &keyboard_button);

}  // namespace td
