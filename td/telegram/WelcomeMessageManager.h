//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/DialogId.h"
#include "td/telegram/EphemeralMessageFullId.h"
#include "td/telegram/EphemeralMessageId.h"
#include "td/telegram/files/FileId.h"
#include "td/telegram/files/FileSourceId.h"
#include "td/telegram/MessageContentUploadId.h"
#include "td/telegram/td_api.h"
#include "td/telegram/telegram_api.h"

#include "td/actor/actor.h"

#include "td/utils/common.h"
#include "td/utils/FlatHashMap.h"
#include "td/utils/FlatHashSet.h"
#include "td/utils/Promise.h"
#include "td/utils/Status.h"

namespace td {

class MessageContent;
class Td;

class WelcomeMessageManager final : public Actor {
 public:
  WelcomeMessageManager(Td *td, ActorShared<> parent);

  void on_external_update_message_content(EphemeralMessageFullId message_full_id, const char *source,
                                          bool expect_no_message = false) const;

  void delete_pending_message_web_page(EphemeralMessageFullId message_full_id);

  void load_welcome_messages(DialogId dialog_id, Promise<Unit> &&promise);

  void reload_welcome_messages(DialogId dialog_id, Promise<Unit> &&promise);

  void drop_welcome_messages(DialogId dialog_id);

  void on_new_welcome_message(telegram_api::object_ptr<telegram_api::ephemeralMessage> &&message);

  void on_edited_welcome_message(telegram_api::object_ptr<telegram_api::ephemeralMessage> &&message);

  void on_delete_welcome_messages(DialogId dialog_id, vector<EphemeralMessageId> ephemeral_message_ids);

  FileSourceId get_welcome_messages_file_source_id(DialogId dialog_id);

  void get_current_state(vector<td_api::object_ptr<td_api::Update>> &updates) const;

 private:
  struct WelcomeMessage {
    EphemeralMessageId ephemeral_message_id_;
    bool invert_media_ = false;
    bool disable_web_page_preview_ = false;
    unique_ptr<MessageContent> content_;

    MessageContentUploadId upload_id_;  // for add_welcome_message/edit_welcome_message

    WelcomeMessage() = default;
    WelcomeMessage(const WelcomeMessage &) = delete;
    WelcomeMessage &operator=(const WelcomeMessage &) = delete;
    WelcomeMessage(WelcomeMessage &&) = delete;
    WelcomeMessage &operator=(WelcomeMessage &&) = delete;
    ~WelcomeMessage();
  };

  struct WelcomeMessageInfo {
    DialogId dialog_id_;
    unique_ptr<WelcomeMessage> message_;
  };

  void tear_down() final;

  Status can_access_welcome_messages(DialogId dialog_id);

  void register_welcome_message(DialogId dialog_id, const WelcomeMessage *m, const char *source);

  void unregister_welcome_message(DialogId dialog_id, const WelcomeMessage *m, const char *source);

  static WelcomeMessageInfo parse_welcome_message(Td *td,
                                                  telegram_api::object_ptr<telegram_api::ephemeralMessage> message,
                                                  const char *source);

  const vector<unique_ptr<WelcomeMessage>> *get_welcome_messages(DialogId dialog_id) const;

  vector<unique_ptr<WelcomeMessage>> *get_welcome_messages(DialogId dialog_id);

  const WelcomeMessage *get_welcome_message(DialogId dialog_id, EphemeralMessageId ephemeral_message_id) const;

  WelcomeMessage *get_welcome_message(DialogId dialog_id, EphemeralMessageId ephemeral_message_id);

  void update_welcome_message_content(WelcomeMessage *old_message, WelcomeMessage *new_message, DialogId dialog_id,
                                      bool &is_content_changed, bool &need_update);

  void on_get_welcome_messages(DialogId dialog_id,
                               Result<telegram_api::object_ptr<telegram_api::ephemeral_WelcomeMessages>> r_messages);

  void do_delete_welcome_messages(DialogId dialog_id, vector<EphemeralMessageId> ephemeral_message_ids);

  vector<FileId> get_dialog_welcome_message_file_ids(const vector<unique_ptr<WelcomeMessage>> &messages) const;

  td_api::object_ptr<td_api::welcomeMessage> get_welcome_message_object(const WelcomeMessage *m) const;

  vector<td_api::object_ptr<td_api::welcomeMessage>> get_welcome_messages_object(
      const vector<unique_ptr<WelcomeMessage>> &messages) const;

  td_api::object_ptr<td_api::updateChatWelcomeMessages> get_update_chat_welcome_messages_object(
      DialogId dialog_id, const vector<unique_ptr<WelcomeMessage>> &messages) const;

  void send_update_chat_welcome_messages(DialogId dialog_id) const;

  Td *td_;
  ActorShared<> parent_;

  FlatHashMap<DialogId, vector<unique_ptr<WelcomeMessage>>, DialogIdHash> welcome_messages_;

  FlatHashMap<DialogId, vector<Promise<Unit>>, DialogIdHash> reload_welcome_messages_queries_;

  FlatHashSet<DialogId, DialogIdHash> loaded_welcome_messages_;

  FlatHashSet<EphemeralMessageFullId, EphemeralMessageFullIdHash> deleted_welcome_messages_;

  FlatHashMap<DialogId, FileSourceId, DialogIdHash> dialog_to_file_source_id_;
};

}  // namespace td
