//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/RichButtonStyle.h"

namespace td {

RichButtonStyle::RichButtonStyle(td_api::object_ptr<td_api::ButtonStyle> &&style, bool is_link) : is_link_(is_link) {
  if (style == nullptr) {
    return;
  }
  switch (style->get_id()) {
    case td_api::buttonStyleDefault::ID:
      break;
    case td_api::buttonStylePrimary::ID:
      type_ = Type::Primary;
      break;
    case td_api::buttonStyleDanger::ID:
      type_ = Type::Danger;
      break;
    case td_api::buttonStyleSuccess::ID:
      type_ = Type::Success;
      break;
    default:
      UNREACHABLE();
      break;
  }
}

RichButtonStyle::RichButtonStyle(telegram_api::object_ptr<telegram_api::richButtonStyle> &&style) {
  if (style == nullptr) {
    return;
  }
  if (style->bg_primary_) {
    type_ = Type::Primary;
  } else if (style->bg_danger_) {
    type_ = Type::Danger;
  } else if (style->bg_success_) {
    type_ = Type::Success;
  }
  is_link_ = style->link_;
}

td_api::object_ptr<td_api::ButtonStyle> RichButtonStyle::get_button_style_object() const {
  switch (type_) {
    case Type::Default:
      return td_api::make_object<td_api::buttonStyleDefault>();
    case Type::Primary:
      return td_api::make_object<td_api::buttonStylePrimary>();
    case Type::Danger:
      return td_api::make_object<td_api::buttonStyleDanger>();
    case Type::Success:
      return td_api::make_object<td_api::buttonStyleSuccess>();
    default:
      UNREACHABLE();
      return nullptr;
  }
}

telegram_api::object_ptr<telegram_api::richButtonStyle> RichButtonStyle::get_input_rich_button_style() const {
  if (is_default()) {
    return nullptr;
  }
  return telegram_api::make_object<telegram_api::richButtonStyle>(0, type_ == Type::Primary, type_ == Type::Danger,
                                                                  type_ == Type::Success, is_link_);
}

bool operator==(const RichButtonStyle &lhs, const RichButtonStyle &rhs) {
  return lhs.type_ == rhs.type_ && lhs.is_link_ == rhs.is_link_;
}

StringBuilder &operator<<(StringBuilder &string_builder, const RichButtonStyle &style) {
  if (style.is_default()) {
    return string_builder;
  }
  string_builder << ", ";
  switch (style.type_) {
    case RichButtonStyle::Type::Default:
      string_builder << "Default";
      break;
    case RichButtonStyle::Type::Primary:
      string_builder << "Primary";
      break;
    case RichButtonStyle::Type::Danger:
      string_builder << "Danger";
      break;
    case RichButtonStyle::Type::Success:
      string_builder << "Success";
      break;
    default:
      UNREACHABLE();
  }
  if (style.is_link_) {
    string_builder << " link";
  }
  return string_builder;
}

}  // namespace td
