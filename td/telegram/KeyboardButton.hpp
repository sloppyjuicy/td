//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/KeyboardButton.h"
#include "td/telegram/KeyboardButtonStyle.hpp"
#include "td/telegram/RequestedDialogType.hpp"
#include "td/telegram/Version.h"

#include "td/utils/tl_helpers.h"

namespace td {

template <class StorerT>
void store(const KeyboardButton &button, StorerT &storer) {
  bool has_url = !button.url_.empty();
  bool has_requested_dialog_type = button.requested_dialog_type_ != nullptr;
  bool has_style = !button.style_.is_default();
  BEGIN_STORE_FLAGS();
  STORE_FLAG(has_url);
  STORE_FLAG(has_requested_dialog_type);
  STORE_FLAG(has_style);
  END_STORE_FLAGS();
  store(button.type_, storer);
  store(button.text_, storer);
  if (has_url) {
    store(button.url_, storer);
  }
  if (has_requested_dialog_type) {
    store(button.requested_dialog_type_, storer);
  }
  if (has_style) {
    store(button.style_, storer);
  }
}

template <class ParserT>
void parse(KeyboardButton &button, ParserT &parser) {
  bool has_url;
  bool has_requested_dialog_type;
  bool has_style;
  if (parser.version() >= static_cast<int32>(Version::AddKeyboardButtonFlags)) {
    BEGIN_PARSE_FLAGS();
    PARSE_FLAG(has_url);
    PARSE_FLAG(has_requested_dialog_type);
    PARSE_FLAG(has_style);
    END_PARSE_FLAGS();
  } else {
    has_url = false;
    has_requested_dialog_type = false;
    has_style = false;
  }
  parse(button.type_, parser);
  parse(button.text_, parser);
  if (has_url) {
    parse(button.url_, parser);
  }
  if (has_requested_dialog_type) {
    parse(button.requested_dialog_type_, parser);
  }
  if (has_style) {
    parse(button.style_, parser);
  }
}

}  // namespace td
