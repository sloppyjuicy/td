//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/WelcomeMessageManager.h"

#include "td/telegram/AccessRights.h"
#include "td/telegram/AuthManager.h"
#include "td/telegram/DialogId.h"
#include "td/telegram/DialogManager.h"
#include "td/telegram/files/FileUploadId.h"
#include "td/telegram/Global.h"
#include "td/telegram/MessageContent.h"
#include "td/telegram/MessageId.h"
#include "td/telegram/MessageSelfDestructType.h"
#include "td/telegram/Td.h"
#include "td/telegram/UserId.h"

#include "td/utils/algorithm.h"
#include "td/utils/logging.h"
#include "td/utils/Status.h"

namespace td {

class GetWelcomeMessagesQuery final : public Td::ResultHandler {
  Promise<telegram_api::object_ptr<telegram_api::ephemeral_WelcomeMessages>> promise_;
  DialogId dialog_id_;

 public:
  explicit GetWelcomeMessagesQuery(Promise<telegram_api::object_ptr<telegram_api::ephemeral_WelcomeMessages>> &&promise)
      : promise_(std::move(promise)) {
  }

  void send(DialogId dialog_id) {
    dialog_id_ = dialog_id;
    auto input_peer = td_->dialog_manager_->get_input_peer(dialog_id, AccessRights::Read);
    if (input_peer == nullptr) {
      return on_error(Status::Error(400, "Chat not found"));
    }
    send_query(G()->net_query_creator().create(telegram_api::ephemeral_getWelcomeMessages(std::move(input_peer), 0)));
  }

  void on_result(BufferSlice packet) final {
    auto result_ptr = fetch_result<telegram_api::ephemeral_getWelcomeMessages>(packet);
    if (result_ptr.is_error()) {
      return on_error(result_ptr.move_as_error());
    }

    auto ptr = result_ptr.move_as_ok();
    LOG(INFO) << "Receive result for GetWelcomeMessagesQuery: " << to_string(ptr);
    promise_.set_value(std::move(ptr));
  }

  void on_error(Status status) final {
    td_->dialog_manager_->on_get_dialog_error(dialog_id_, status, "GetWelcomeMessagesQuery");
    promise_.set_error(std::move(status));
  }
};

WelcomeMessageManager::WelcomeMessage::~WelcomeMessage() = default;

WelcomeMessageManager::WelcomeMessageManager(Td *td, ActorShared<> parent) : td_(td), parent_(std::move(parent)) {
}

void WelcomeMessageManager::tear_down() {
  parent_.reset();
}

Status WelcomeMessageManager::can_access_welcome_messages(DialogId dialog_id) {
  TRY_STATUS(
      td_->dialog_manager_->check_dialog_access(dialog_id, false, AccessRights::Write, "can_access_welcome_messages"));
  if (dialog_id.get_type() == DialogType::User) {
    return Status::Error(400, "Chat can't have welcome messages");
  }
  if (!td_->dialog_manager_->get_dialog_status(dialog_id).can_change_info_and_settings_as_administrator()) {
    return Status::Error(400, "Have no enough rights");
  }
  return Status::OK();
}

WelcomeMessageManager::WelcomeMessageInfo WelcomeMessageManager::parse_welcome_message(
    Td *td, telegram_api::object_ptr<telegram_api::ephemeralMessage> message, const char *source) {
  LOG(DEBUG) << "Receive from " << source << ' ' << to_string(message);
  CHECK(message != nullptr);

  WelcomeMessageInfo message_info;
  if (!message->welcome_template_) {
    LOG(ERROR) << "Receive non-welcome message from " << source;
    return message_info;
  }
  if (message->peer_id_ == nullptr) {
    LOG(ERROR) << "Receive welcome message without chat from " << source;
    return message_info;
  }
  auto dialog_id = DialogId(message->peer_id_);
  auto ephemeral_message_id = EphemeralMessageId(message->id_);
  if (!dialog_id.is_valid() || !ephemeral_message_id.is_valid()) {
    LOG(ERROR) << "Ignore " << ephemeral_message_id << " in " << dialog_id << " from " << source;
    return message_info;
  }
  message_info.dialog_id_ = dialog_id;
  message_info.message_ = make_unique<WelcomeMessage>();
  auto *m = message_info.message_.get();
  m->ephemeral_message_id_ = ephemeral_message_id;
  m->invert_media_ = message->invert_media_;
  m->content_ = get_message_content(
      td,
      get_message_text(td->user_manager_.get(), std::move(message->message_), std::move(message->entities_), true,
                       td->auth_manager_->is_bot(), 0, false, source),
      std::move(message->rich_message_), std::move(message->media_), dialog_id, 0, true, UserId(), nullptr,
      &m->disable_web_page_preview_, source);
  // m->reply_markup = std::move(message->reply_markup_);
  return message_info;
}

void WelcomeMessageManager::on_new_welcome_message(telegram_api::object_ptr<telegram_api::ephemeralMessage> &&message) {
  auto message_info = parse_welcome_message(td_, std::move(message), "on_new_welcome_message");
  auto dialog_id = message_info.dialog_id_;
  if (!dialog_id.is_valid() || can_access_welcome_messages(dialog_id).is_error() ||
      loaded_welcome_messages_.count(dialog_id) == 0) {
    return;
  }

  auto ephemeral_message_id = message_info.message_->ephemeral_message_id_;
  if (get_welcome_message(dialog_id, ephemeral_message_id) != nullptr ||
      deleted_welcome_messages_.count({dialog_id, ephemeral_message_id}) != 0) {
    return;
  }
  auto &messages = welcome_messages_[dialog_id];
  messages.push_back(std::move(message_info.message_));
  send_update_chat_welcome_messages_object(dialog_id);
  reload_welcome_messages(dialog_id, Promise<Unit>());
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

void WelcomeMessageManager::load_welcome_messages(DialogId dialog_id, Promise<Unit> &&promise) {
  TRY_STATUS_PROMISE(promise, can_access_welcome_messages(dialog_id));
  if (loaded_welcome_messages_.count(dialog_id)) {
    promise.set_value(Unit());
    promise = Promise<Unit>();
  }
  reload_welcome_messages(dialog_id, std::move(promise));
}

void WelcomeMessageManager::reload_welcome_messages(DialogId dialog_id, Promise<Unit> &&promise) {
  CHECK(dialog_id.is_valid());
  TRY_STATUS_PROMISE(promise, can_access_welcome_messages(dialog_id));
  auto &queries = reload_welcome_messages_queries_[dialog_id];
  queries.push_back(std::move(promise));
  if (queries.size() == 1u) {
    auto query_promise = PromiseCreator::lambda(
        [actor_id = actor_id(this),
         dialog_id](Result<telegram_api::object_ptr<telegram_api::ephemeral_WelcomeMessages>> r_messages) {
          send_closure(actor_id, &WelcomeMessageManager::on_get_welcome_messages, dialog_id, std::move(r_messages));
        });
    td_->create_handler<GetWelcomeMessagesQuery>(std::move(query_promise))->send(dialog_id);
  }
}

void WelcomeMessageManager::on_get_welcome_messages(
    DialogId dialog_id, Result<telegram_api::object_ptr<telegram_api::ephemeral_WelcomeMessages>> r_messages) {
  G()->ignore_result_if_closing(r_messages);
  auto it = reload_welcome_messages_queries_.find(dialog_id);
  CHECK(it != reload_welcome_messages_queries_.end());
  auto promises = std::move(it->second);
  CHECK(!promises.empty());
  reload_welcome_messages_queries_.erase(it);

  if (r_messages.is_error()) {
    return fail_promises(promises, r_messages.move_as_error());
  }
  auto ephemeral_messages_ptr = r_messages.move_as_ok();
  if (ephemeral_messages_ptr->get_id() != telegram_api::ephemeral_welcomeMessages::ID) {
    LOG(ERROR) << "Receive " << to_string(ephemeral_messages_ptr);
    return fail_promises(promises, Status::Error(500, "Receive invalid response"));
  }
  auto ephemeral_messages =
      telegram_api::move_object_as<telegram_api::ephemeral_welcomeMessages>(ephemeral_messages_ptr);
  vector<unique_ptr<WelcomeMessage>> welcome_messages;
  bool need_update = false;
  bool is_content_changed = false;
  for (auto &message : ephemeral_messages->messages_) {
    auto message_info = parse_welcome_message(td_, std::move(message), "on_get_welcome_messages");
    if (dialog_id != message_info.dialog_id_) {
      LOG(ERROR) << "Receive welcome message in " << message_info.dialog_id_ << " instead of " << dialog_id;
      return fail_promises(promises, Status::Error(500, "Receive invalid response"));
    }
    auto *old_message = get_welcome_message(dialog_id, message_info.message_->ephemeral_message_id_);
    if (old_message != nullptr) {
      merge_and_compare_message_contents(td_, old_message->content_.get(), message_info.message_->content_.get(), false,
                                         dialog_id, false, vector<FileUploadId>(), MessageSelfDestructType(), 0.0,
                                         nullptr, is_content_changed, need_update);
    }
    welcome_messages.push_back(std::move(message_info.message_));
  }
  if (loaded_welcome_messages_.insert(dialog_id).second) {
    need_update = true;
  }

  if (welcome_messages.empty()) {
    if (welcome_messages_.erase(dialog_id) != 0) {
      need_update = true;
    }
  } else {
    auto &messages = welcome_messages_[dialog_id];
    if (messages.size() != welcome_messages.size()) {
      need_update = true;
    } else {
      for (size_t i = 0; i < messages.size(); i++) {
        if (messages[i]->ephemeral_message_id_ != welcome_messages[i]->ephemeral_message_id_) {
          need_update = true;
        }
      }
    }
    if (need_update || is_content_changed) {
      messages = std::move(welcome_messages);
    }
  }
  if (need_update) {
    send_update_chat_welcome_messages_object(dialog_id);
  }
  set_promises(promises);
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

void WelcomeMessageManager::get_current_state(vector<td_api::object_ptr<td_api::Update>> &updates) const {
  if (!td_->auth_manager_->is_authorized() || td_->auth_manager_->is_bot()) {
    return;
  }

  for (auto &it : welcome_messages_) {
    updates.push_back(get_update_chat_welcome_messages_object(it.first, it.second));
  }
}

}  // namespace td
