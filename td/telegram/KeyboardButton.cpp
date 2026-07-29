//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/KeyboardButton.h"

#include "td/telegram/Global.h"
#include "td/telegram/LinkManager.h"
#include "td/telegram/misc.h"

#include "td/utils/logging.h"

namespace td {

KeyboardButton get_keyboard_button(telegram_api::object_ptr<telegram_api::keyboardButton> &&keyboard_button) {
  CHECK(keyboard_button != nullptr);

  KeyboardButton button;
  switch (keyboard_button->type_->get_id()) {
    case telegram_api::buttonTypeDefault::ID:
      button.type = KeyboardButton::Type::Text;
      break;
    case telegram_api::buttonTypeRequestPhone::ID:
      button.type = KeyboardButton::Type::RequestPhoneNumber;
      break;
    case telegram_api::buttonTypeRequestGeoLocation::ID:
      button.type = KeyboardButton::Type::RequestLocation;
      break;
    case telegram_api::buttonTypeRequestPoll::ID: {
      auto type = telegram_api::move_object_as<telegram_api::buttonTypeRequestPoll>(keyboard_button->type_);
      if ((type->flags_ & telegram_api::buttonTypeRequestPoll::QUIZ_MASK) != 0) {
        if (type->quiz_) {
          button.type = KeyboardButton::Type::RequestPollQuiz;
        } else {
          button.type = KeyboardButton::Type::RequestPollRegular;
        }
      } else {
        button.type = KeyboardButton::Type::RequestPoll;
      }
      break;
    }
    case telegram_api::buttonTypeSimpleWebView::ID: {
      auto type = telegram_api::move_object_as<telegram_api::buttonTypeSimpleWebView>(keyboard_button->type_);
      auto r_url = LinkManager::check_link(type->url_);
      if (r_url.is_error()) {
        LOG(ERROR) << "Keyboard Web App " << r_url.error().message();
        break;
      }

      button.type = KeyboardButton::Type::WebView;
      button.url = r_url.move_as_ok();
      break;
    }
    case telegram_api::buttonTypeRequestPeer::ID: {
      auto type = telegram_api::move_object_as<telegram_api::buttonTypeRequestPeer>(keyboard_button->type_);
      button.type = KeyboardButton::Type::RequestDialog;
      button.requested_dialog_type =
          td::make_unique<RequestedDialogType>(std::move(type->peer_type_), type->button_id_, type->max_quantity_);
      break;
    }
    default:
      LOG(ERROR) << "Unsupported keyboard button: " << to_string(keyboard_button->type_);
  }
  button.style = KeyboardButtonStyle(std::move(keyboard_button->style_));
  button.text = std::move(keyboard_button->text_);
  return button;
}

Result<KeyboardButton> get_keyboard_button(td_api::object_ptr<td_api::keyboardButton> &&button,
                                           bool request_buttons_allowed) {
  CHECK(button != nullptr);

  if (!clean_input_string(button->text_)) {
    return Status::Error(400, "Keyboard button text must be encoded in UTF-8");
  }
  if (button->text_.empty()) {
    return Status::Error(400, "Keyboard button text must be non-empty");
  }

  KeyboardButton current_button;
  current_button.text = std::move(button->text_);
  current_button.style = KeyboardButtonStyle(std::move(button->style_), button->icon_custom_emoji_id_);

  switch (button->type_ == nullptr ? td_api::keyboardButtonTypeText::ID : button->type_->get_id()) {
    case td_api::keyboardButtonTypeText::ID:
      current_button.type = KeyboardButton::Type::Text;
      break;
    case td_api::keyboardButtonTypeRequestPhoneNumber::ID:
      if (!request_buttons_allowed) {
        return Status::Error(400, "Phone number can be requested in private chats only");
      }
      current_button.type = KeyboardButton::Type::RequestPhoneNumber;
      break;
    case td_api::keyboardButtonTypeRequestLocation::ID:
      if (!request_buttons_allowed) {
        return Status::Error(400, "Location can be requested in private chats only");
      }
      current_button.type = KeyboardButton::Type::RequestLocation;
      break;
    case td_api::keyboardButtonTypeRequestPoll::ID: {
      if (!request_buttons_allowed) {
        return Status::Error(400, "Poll can be requested in private chats only");
      }
      auto *request_poll = static_cast<const td_api::keyboardButtonTypeRequestPoll *>(button->type_.get());
      if (request_poll->force_quiz_ && request_poll->force_regular_) {
        return Status::Error(400, "Can't force quiz mode and regular poll simultaneously");
      }
      if (request_poll->force_quiz_) {
        current_button.type = KeyboardButton::Type::RequestPollQuiz;
      } else if (request_poll->force_regular_) {
        current_button.type = KeyboardButton::Type::RequestPollRegular;
      } else {
        current_button.type = KeyboardButton::Type::RequestPoll;
      }
      break;
    }
    case td_api::keyboardButtonTypeWebApp::ID: {
      if (!request_buttons_allowed) {
        return Status::Error(400, "Web App buttons can be used in private chats only");
      }

      auto button_type = move_tl_object_as<td_api::keyboardButtonTypeWebApp>(button->type_);
      auto user_id = LinkManager::get_link_user_id(button_type->url_);
      if (user_id.is_valid()) {
        return Status::Error(400, "Link to a user can't be used in Web App URL buttons");
      }
      auto r_url = LinkManager::check_link(button_type->url_, true, !G()->is_test_dc());
      if (r_url.is_error()) {
        return Status::Error(400, PSLICE() << "Keyboard button Web App " << r_url.error().message());
      }
      current_button.type = KeyboardButton::Type::WebView;
      current_button.url = std::move(button_type->url_);
      break;
    }
    case td_api::keyboardButtonTypeRequestUsers::ID: {
      if (!request_buttons_allowed) {
        return Status::Error(400, "Users can be requested in private chats only");
      }
      auto button_type = move_tl_object_as<td_api::keyboardButtonTypeRequestUsers>(button->type_);
      current_button.type = KeyboardButton::Type::RequestDialog;
      current_button.requested_dialog_type = td::make_unique<RequestedDialogType>(std::move(button_type));
      break;
    }
    case td_api::keyboardButtonTypeRequestChat::ID: {
      if (!request_buttons_allowed) {
        return Status::Error(400, "Chats can be requested in private chats only");
      }
      auto button_type = move_tl_object_as<td_api::keyboardButtonTypeRequestChat>(button->type_);
      current_button.type = KeyboardButton::Type::RequestDialog;
      current_button.requested_dialog_type = td::make_unique<RequestedDialogType>(std::move(button_type));
      break;
    }
    case td_api::keyboardButtonTypeRequestManagedBot::ID: {
      if (!request_buttons_allowed) {
        return Status::Error(400, "Managed bots can be requested in private chats only");
      }
      auto button_type = move_tl_object_as<td_api::keyboardButtonTypeRequestManagedBot>(button->type_);
      current_button.type = KeyboardButton::Type::RequestDialog;
      current_button.requested_dialog_type = td::make_unique<RequestedDialogType>(std::move(button_type));
      break;
    }
    default:
      UNREACHABLE();
  }
  return std::move(current_button);
}

telegram_api::object_ptr<telegram_api::keyboardButton> get_input_keyboard_button(
    const KeyboardButton &keyboard_button) {
  auto type = [&]() -> telegram_api::object_ptr<telegram_api::ButtonType> {
    switch (keyboard_button.type) {
      case KeyboardButton::Type::Text:
        return telegram_api::make_object<telegram_api::buttonTypeDefault>();
      case KeyboardButton::Type::RequestPhoneNumber:
        return telegram_api::make_object<telegram_api::buttonTypeRequestPhone>();
      case KeyboardButton::Type::RequestLocation:
        return telegram_api::make_object<telegram_api::buttonTypeRequestGeoLocation>();
      case KeyboardButton::Type::RequestPoll:
        return telegram_api::make_object<telegram_api::buttonTypeRequestPoll>(0, false);
      case KeyboardButton::Type::RequestPollQuiz:
        return telegram_api::make_object<telegram_api::buttonTypeRequestPoll>(
            telegram_api::buttonTypeRequestPoll::QUIZ_MASK, true);
      case KeyboardButton::Type::RequestPollRegular:
        return telegram_api::make_object<telegram_api::buttonTypeRequestPoll>(
            telegram_api::buttonTypeRequestPoll::QUIZ_MASK, false);
      case KeyboardButton::Type::WebView:
        return telegram_api::make_object<telegram_api::buttonTypeSimpleWebView>(keyboard_button.url);
      case KeyboardButton::Type::RequestDialog:
        CHECK(keyboard_button.requested_dialog_type != nullptr);
        return keyboard_button.requested_dialog_type->get_input_button_type_request_peer();
      default:
        UNREACHABLE();
        return nullptr;
    }
  }();
  int32 flags = 0;
  auto style = keyboard_button.style.get_input_keyboard_button_style();
  if (style != nullptr) {
    flags |= 1 << 10;
  }
  return telegram_api::make_object<telegram_api::keyboardButton>(flags, std::move(style), keyboard_button.text,
                                                                 std::move(type));
}

td_api::object_ptr<td_api::keyboardButton> get_keyboard_button_object(const KeyboardButton &keyboard_button) {
  td_api::object_ptr<td_api::KeyboardButtonType> type;
  switch (keyboard_button.type) {
    case KeyboardButton::Type::Text:
      type = make_tl_object<td_api::keyboardButtonTypeText>();
      break;
    case KeyboardButton::Type::RequestPhoneNumber:
      type = make_tl_object<td_api::keyboardButtonTypeRequestPhoneNumber>();
      break;
    case KeyboardButton::Type::RequestLocation:
      type = make_tl_object<td_api::keyboardButtonTypeRequestLocation>();
      break;
    case KeyboardButton::Type::RequestPoll:
      type = make_tl_object<td_api::keyboardButtonTypeRequestPoll>(false, false);
      break;
    case KeyboardButton::Type::RequestPollQuiz:
      type = make_tl_object<td_api::keyboardButtonTypeRequestPoll>(false, true);
      break;
    case KeyboardButton::Type::RequestPollRegular:
      type = make_tl_object<td_api::keyboardButtonTypeRequestPoll>(true, false);
      break;
    case KeyboardButton::Type::WebView:
      type = make_tl_object<td_api::keyboardButtonTypeWebApp>(keyboard_button.url + "#kb");
      break;
    case KeyboardButton::Type::RequestDialog:
      type = keyboard_button.requested_dialog_type->get_keyboard_button_type_object();
      break;
    default:
      UNREACHABLE();
      return nullptr;
  }
  return td_api::make_object<td_api::keyboardButton>(keyboard_button.text,
                                                     keyboard_button.style.get_icon_custom_emoji_id().get(),
                                                     keyboard_button.style.get_button_style_object(), std::move(type));
}

bool operator==(const KeyboardButton &lhs, const KeyboardButton &rhs) {
  return lhs.type == rhs.type && lhs.style == rhs.style && lhs.text == rhs.text && lhs.url == rhs.url;
}

StringBuilder &operator<<(StringBuilder &string_builder, const KeyboardButton &keyboard_button) {
  string_builder << "Button[";
  switch (keyboard_button.type) {
    case KeyboardButton::Type::Text:
      string_builder << "Text";
      break;
    case KeyboardButton::Type::RequestPhoneNumber:
      string_builder << "RequestPhoneNumber";
      break;
    case KeyboardButton::Type::RequestLocation:
      string_builder << "RequestLocation";
      break;
    case KeyboardButton::Type::RequestPoll:
      string_builder << "RequestPoll";
      break;
    case KeyboardButton::Type::RequestPollQuiz:
      string_builder << "RequestPollQuiz";
      break;
    case KeyboardButton::Type::RequestPollRegular:
      string_builder << "RequestPollRegular";
      break;
    case KeyboardButton::Type::WebView:
      string_builder << "WebApp";
      break;
    case KeyboardButton::Type::RequestDialog:
      string_builder << "RequestChat";
      break;
    default:
      UNREACHABLE();
  }
  return string_builder << ", " << keyboard_button.text << keyboard_button.style << ']';
}

}  // namespace td
