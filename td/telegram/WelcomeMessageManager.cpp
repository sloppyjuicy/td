//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2026
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/WelcomeMessageManager.h"

#include "td/telegram/DialogId.h"
#include "td/telegram/MessageContent.h"
#include "td/telegram/MessageId.h"
#include "td/telegram/Td.h"
#include "td/telegram/UserId.h"

namespace td {

WelcomeMessageManager::WelcomeMessage::~WelcomeMessage() = default;

WelcomeMessageManager::WelcomeMessageManager(Td *td, ActorShared<> parent) : td_(td), parent_(std::move(parent)) {
}

void WelcomeMessageManager::tear_down() {
  parent_.reset();
}

td_api::object_ptr<td_api::welcomeMessage> WelcomeMessageManager::get_welcome_message_object(
    const WelcomeMessage *m) const {
  CHECK(m != nullptr);
  auto content = get_message_content_object(m->content_.get(), td_, DialogId(), MessageId(), DialogId(), false, false,
                                            false, DialogId(), 0, 0, false, true, -1, m->invert_media_,
                                            m->disable_web_page_preview_, "get_welcome_message_object");
  return td_api::make_object<td_api::welcomeMessage>(m->ephemeral_message_id_.get(), std::move(content));
}

}  // namespace td
