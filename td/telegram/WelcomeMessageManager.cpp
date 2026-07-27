//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/WelcomeMessageManager.h"

#include "td/telegram/DialogId.h"
#include "td/telegram/DialogManager.h"
#include "td/telegram/Global.h"
#include "td/telegram/MessageContent.h"
#include "td/telegram/MessageId.h"
#include "td/telegram/Td.h"
#include "td/telegram/UserId.h"

#include "td/utils/algorithm.h"

namespace td {

WelcomeMessageManager::WelcomeMessage::~WelcomeMessage() = default;

WelcomeMessageManager::WelcomeMessageManager(Td *td, ActorShared<> parent) : td_(td), parent_(std::move(parent)) {
}

void WelcomeMessageManager::tear_down() {
  parent_.reset();
}

const vector<unique_ptr<WelcomeMessageManager::WelcomeMessage>> *WelcomeMessageManager::get_welcome_messages(
    DialogId dialog_id) const {
  auto it = welcome_messages_.find(dialog_id);
  if (it == welcome_messages_.end()) {
    return nullptr;
  }
  return &it->second;
}

vector<unique_ptr<WelcomeMessageManager::WelcomeMessage>> *WelcomeMessageManager::get_welcome_messages(
    DialogId dialog_id) {
  auto it = welcome_messages_.find(dialog_id);
  if (it == welcome_messages_.end()) {
    return nullptr;
  }
  return &it->second;
}

const WelcomeMessageManager::WelcomeMessage *WelcomeMessageManager::get_welcome_message(
    DialogId dialog_id, EphemeralMessageId ephemeral_message_id) const {
  auto messages = get_welcome_messages(dialog_id);
  if (messages != nullptr) {
    for (auto &message : *messages) {
      if (message->ephemeral_message_id_ == ephemeral_message_id) {
        return message.get();
      }
    }
  }
  return nullptr;
}

WelcomeMessageManager::WelcomeMessage *WelcomeMessageManager::get_welcome_message(
    DialogId dialog_id, EphemeralMessageId ephemeral_message_id) {
  auto messages = get_welcome_messages(dialog_id);
  if (messages != nullptr) {
    for (auto &message : *messages) {
      if (message->ephemeral_message_id_ == ephemeral_message_id) {
        return message.get();
      }
    }
  }
  return nullptr;
}

td_api::object_ptr<td_api::welcomeMessage> WelcomeMessageManager::get_welcome_message_object(
    const WelcomeMessage *m) const {
  CHECK(m != nullptr);
  auto content = get_message_content_object(m->content_.get(), td_, DialogId(), MessageId(), DialogId(), false, false,
                                            false, DialogId(), 0, 0, false, true, -1, m->invert_media_,
                                            m->disable_web_page_preview_, "get_welcome_message_object");
  return td_api::make_object<td_api::welcomeMessage>(m->ephemeral_message_id_.get(), std::move(content));
}

vector<td_api::object_ptr<td_api::welcomeMessage>> WelcomeMessageManager::get_welcome_messages_object(
    const vector<unique_ptr<WelcomeMessage>> &messages) const {
  return transform(
      messages, [&](const unique_ptr<WelcomeMessage> &message) { return get_welcome_message_object(message.get()); });
}

td_api::object_ptr<td_api::updateChatWelcomeMessages> WelcomeMessageManager::get_update_chat_welcome_messages_object(
    DialogId dialog_id, const vector<unique_ptr<WelcomeMessage>> &messages) const {
  return td_api::make_object<td_api::updateChatWelcomeMessages>(
      td_->dialog_manager_->get_chat_id_object(dialog_id, "updateChatWelcomeMessages"),
      get_welcome_messages_object(messages));
}

void WelcomeMessageManager::send_update_chat_welcome_messages_object(DialogId dialog_id) const {
  auto messages = get_welcome_messages(dialog_id);
  if (messages == nullptr) {
    send_closure(G()->td(), &Td::send_update,
                 get_update_chat_welcome_messages_object(dialog_id, vector<unique_ptr<WelcomeMessage>>()));
  } else {
    send_closure(G()->td(), &Td::send_update, get_update_chat_welcome_messages_object(dialog_id, *messages));
  }
}

}  // namespace td
