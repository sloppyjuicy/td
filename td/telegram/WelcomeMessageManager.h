//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/EphemeralMessageId.h"
#include "td/telegram/MessageContentUploadId.h"
#include "td/telegram/td_api.h"

#include "td/actor/actor.h"

#include "td/utils/common.h"

namespace td {

class MessageContent;
class Td;

class WelcomeMessageManager final : public Actor {
 public:
  WelcomeMessageManager(Td *td, ActorShared<> parent);

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

  void tear_down() final;

  td_api::object_ptr<td_api::welcomeMessage> get_welcome_message_object(const WelcomeMessage *m) const;

  Td *td_;
  ActorShared<> parent_;
};

}  // namespace td
