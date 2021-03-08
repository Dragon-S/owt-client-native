// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include <thread>
#include <algorithm>
#include <ctime>
#if defined(WEBRTC_IOS)
#include <CoreFoundation/CFDate.h>
#endif
#include "talk/owt/sdk/base/mediautils.h"
#include "talk/owt/sdk/base/stringutils.h"
#include "talk/owt/sdk/base/sysinfo.h"
#include "talk/owt/sdk/conference/conferencesocketsignalingchannel.h"
#include "webrtc/rtc_base/third_party/base64/base64.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/strings/json.h"
#include "webrtc/rtc_base/time_utils.h"
using namespace rtc;
namespace owt {
namespace conference {
#define SIGNALING_PROTOCOL_VERSION "1.0"
const std::string kEventNameCustomMessage = "customMessage";
const std::string kEventNameSignalingMessagePrelude = "signaling";
const std::string kEventNameSignalingMessage = "soac"; //only for soac message
const std::string kEventNameOnSignalingMessage = "progress";
const std::string kEventNameOnCustomMessage = "text";
const std::string kEventNameSubscribe = "subscribe";
const std::string kEventNameUnsubscribe = "unsubscribe";
const std::string kEventNamePublish = "publish";
const std::string kEventNameUnpublish = "unpublish";
const std::string kEventNameStreamControl = "stream-control";
const std::string kEventNameSubscriptionControl = "subscription-control";
const std::string kEventNameGetRegion = "get-region";
const std::string kEventNameSetRegion = "set-region";
const std::string kEventNameMute = "mute";
const std::string kEventNameUnmute = "unmute";
const std::string kEventNameMix = "mix";
const std::string kEventNameUnmix = "unmix";
const std::string kEventNameExternalOutput = "streaming";
const std::string kEventNameRecording = "recording";
const std::string kEventNameRefreshReconnectionTicket =
    "refreshReconnectionTicket";
const std::string kEventNameLogout = "logout";
const std::string kEventNameRelogin = "relogin";
const std::string kEventNameStreamMessage = "stream";
const std::string kEventNameOnAddStream = "add";
const std::string kEventNameOnRemoveStream = "remove";
const std::string kEventNameOnUpdateStream = "update";
const std::string kEventNameOnParticipant = "participant";
const std::string kNotificationNameUserJoin = "join";
const std::string kNotificationNameUserLeave = "leave";
const std::string kEventNameTextMessage = "text";
const std::string kEventNameOnStreamUpdate = "stream";
const std::string kEventNameOnUserJoin = "user_join";
const std::string kEventNameOnUserLeave = "user_leave";
const std::string kEventNameOnUserPresence = "participant";
const std::string kEventNameOnDrop = "drop";
const std::string kEventNameConnectionFailed = "connection_failed";
const std::string kEventNameSipAndPstnJoin = "sipAndPstnJoin";
#if defined(WEBRTC_IOS)
// The epoch of Mach kernel is 2001/1/1 00:00:00, while Linux is 1970/1/1
// 00:00:00.
const uint64_t kMachLinuxTimeDelta = 978307200;
#endif
const int kReconnectionAttempts = 14;
const int kReconnectionDelay = 2000;
const int kReconnectionDelayMax = 5000;
ConferenceSocketSignalingChannel::ConferenceSocketSignalingChannel()
    : socket_client_(new sio::client()),
      reconnection_ticket_(""),
      participant_id_(""),
      reconnection_attempted_(0),
      is_reconnection_(false),
      outgoing_message_id_(1) {}
ConferenceSocketSignalingChannel::~ConferenceSocketSignalingChannel() {
  // delete socket_client_;//TODO: 此处有泄漏，主要是为了暂时解决socket线程阻塞主线程问题
  socket_client_->set_reconnect_attempts(0);
  socket_client_->set_reconnect_delay(0);
  socket_client_->set_reconnect_delay_max(0);

  socket_client_->clear_con_listeners();

  socket_client_->clear_socket_listeners();

  TriggerOnServerDisconnected();
}
void ConferenceSocketSignalingChannel::AddObserver(
    ConferenceSocketSignalingChannelObserver& observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  if (std::find(observers_.begin(), observers_.end(), &observer) !=
      observers_.end()) {
    RTC_LOG(LS_INFO) << "Adding duplicated observer.";
    return;
  }
  observers_.push_back(&observer);
}
void ConferenceSocketSignalingChannel::RemoveObserver(
    ConferenceSocketSignalingChannelObserver& observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.erase(std::remove(observers_.begin(), observers_.end(), &observer),
                   observers_.end());
}
void ConferenceSocketSignalingChannel::Connect(
    const std::string& token,
    std::function<void(sio::message::ptr room_info)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  if (!StringUtils::IsBase64EncodedString(token)) {
    if (on_failure != nullptr) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidToken, "Invalid token."));
      on_failure(std::move(e));
    }
    return;
  }
  std::string token_decoded("");
  if (!rtc::Base64::Decode(token, rtc::Base64::DO_STRICT, &token_decoded,
                           nullptr)) {
    if (on_failure) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidToken, "Invalid token."));
      // TODO: Use async instead.
      on_failure(std::move(e));
    }
    return;
  }
  reconnection_ticket_ = "";
  is_reconnection_ = false;
  Json::Value json_token;
  Json::Reader reader;
  if (!reader.parse(token_decoded, json_token)) {
    RTC_LOG(LS_ERROR) << "Parsing token failed.";
    if (on_failure != nullptr) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidToken, "Invalid token."));
      on_failure(std::move(e));
    }
    return;
  }
  std::string scheme("http://");
  std::string host;
  rtc::GetStringFromJsonObject(json_token, "host", &host);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  socket_client_->socket();
  socket_client_->set_reconnect_attempts(kReconnectionAttempts);
  socket_client_->set_reconnect_delay(kReconnectionDelay);
  socket_client_->set_reconnect_delay_max(kReconnectionDelayMax);
  socket_client_->set_socket_close_listener(
      [weak_this](std::string const& nsp) {
        RTC_LOG(LS_INFO) << "Socket.IO disconnected.";
        auto that = weak_this.lock();
        if (that && (that->reconnection_attempted_ >= kReconnectionAttempts ||
                     that->disconnect_complete_)) {
          that->TriggerOnServerDisconnected();
        }
      });
  socket_client_->set_fail_listener([weak_this]() {
    RTC_LOG(LS_ERROR) << "Socket.IO connection failed.";
    auto that = weak_this.lock();
    if (that) {
      that->DropQueuedMessages();
      if (that->reconnection_attempted_ >= kReconnectionAttempts) {
        that->TriggerOnServerDisconnected();
      }
    }
  });
  socket_client_->set_reconnect_listener(
      [weak_this](const unsigned reconnect_made, const unsigned delay) {
        RTC_LOG(LS_INFO) << "Socket.IO Start Reconnection.";
        auto that = weak_this.lock();
        if (that) {
          that->TriggerOnServerReconnecting();
        }
      });
  socket_client_->set_reconnecting_listener([weak_this]() {
    RTC_LOG(LS_INFO) << "Socket.IO reconnecting.";
    auto that = weak_this.lock();
    if (that) {
      if (that->reconnection_ticket_ != "") {
        // It will be reset when a reconnection is success (open listener) or
        // fail (fail listener).
        that->is_reconnection_ = true;
        that->reconnection_attempted_++;
      }
    }
  });
  socket_client_->set_open_listener([=](void) {
    // At this time the connect failure callback is still in pending list. No
    // need to add a new entry in the pending list.
    if (!is_reconnection_) {
      owt::base::SysInfo sys_info(owt::base::SysInfo::GetInstance());
      sio::message::ptr login_message = sio::object_message::create();
      login_message->get_map()["token"] = sio::string_message::create(token);
      sio::message::ptr ua_message = sio::object_message::create();
      sio::message::ptr sdk_message = sio::object_message::create();
      sdk_message->get_map()["type"] =
          sio::string_message::create(sys_info.sdk.type);
      sdk_message->get_map()["version"] =
          sio::string_message::create(sys_info.sdk.version);
      ua_message->get_map()["sdk"] = sdk_message;
      sio::message::ptr os_message = sio::object_message::create();
      os_message->get_map()["name"] =
          sio::string_message::create(sys_info.os.name);
      os_message->get_map()["version"] =
          sio::string_message::create(sys_info.os.version);
      ua_message->get_map()["os"] = os_message;
      sio::message::ptr runtime_message = sio::object_message::create();
      runtime_message->get_map()["name"] =
          sio::string_message::create(sys_info.runtime.name);
      runtime_message->get_map()["version"] =
          sio::string_message::create(sys_info.runtime.version);
      ua_message->get_map()["runtime"] = runtime_message;
      sio::message::ptr compatibility = sio::string_message::create("1");
      ua_message->get_map()["compatibility"] = compatibility;
      login_message->get_map()["userAgent"] = ua_message;
      std::string protocol_version = SIGNALING_PROTOCOL_VERSION;
      login_message->get_map()["protocol"] = sio::string_message::create(protocol_version);
      Emit("login", login_message,
           [=](sio::message::list const& msg) {
             connect_failure_callback_ = nullptr;
             if (msg.size() < 2) {
               RTC_LOG(LS_ERROR) << "Received unknown message while sending token.";
               if (on_failure != nullptr) {
                 std::unique_ptr<Exception> e(new Exception(
                     ExceptionType::kConferenceInvalidParam,
                     "Received unknown message from server."));
                 on_failure(std::move(e));
               }
               return;
             }
             sio::message::ptr ack =
                 msg.at(0);  // The first element indicates the state.
             std::string state = ack->get_string();
             if (state == "error" || state == "timeout") {
               RTC_LOG(LS_ERROR) << "Server returns " << state
                             << " while joining a conference.";
               if (on_failure != nullptr) {
                 std::string errMsg = "Received error message from server.(state = " + state + ")";
                 std::unique_ptr<Exception> e(new Exception(
                     ExceptionType::kConferenceInvalidParam, errMsg));
                 on_failure(std::move(e));
               }
               return;
             }
             // in signaling protocol 1.0.0, the response contains following info:
             // {id: string(participantid),
             //  user: string(userid),
             //  role: string(participantrole),
             //  permission: object(permission),
             //  room: object(RoomInfo),
             //  reconnectonTicket: undefined or string(ReconnecionTicket).}
             // At present client SDK will only save reconnection ticket and participantid
             // and ignoring other info.
             sio::message::ptr message = msg.at(1);
             auto reconnection_ticket_ptr =
                 message->get_map()["reconnectionTicket"];
             if (reconnection_ticket_ptr) {
               OnReconnectionTicket(reconnection_ticket_ptr->get_string());
             }
             auto participant_id_ptr = message->get_map()["id"];
             if (participant_id_ptr) {
               participant_id_ = participant_id_ptr->get_string();
             }
             if (on_success != nullptr) {
               on_success(message);
             }
           },
           on_failure);
      is_reconnection_ = false;
      reconnection_attempted_ = 0;
    } else {
      socket_client_->socket()->emit(
          kEventNameRelogin, reconnection_ticket_, [weak_this](sio::message::list const& msg) {
            auto that = weak_this.lock();
            if (!that) {
              return;
            }
            if (msg.size() < 2) {
              RTC_LOG(LS_WARNING)
                  << "Received unknown message while reconnection ticket.";
              that->reconnection_attempted_ = kReconnectionAttempts;
              that->socket_client_->close();
              return;
            }
            sio::message::ptr ack =
                msg.at(0);  // The first element indicates the state.
            std::string state = ack->get_string();
            if (state == "error" || state == "timeout") {
              RTC_LOG(LS_WARNING)
                  << "Server returns " << state
                  << " when relogin. Maybe an invalid reconnection ticket.";
              that->reconnection_attempted_ = kReconnectionAttempts;
              that->socket_client_->close();
              return;
            }
            // The second element is room info, please refer to MCU
            // erizoController's implementation for detailed message format.
            sio::message::ptr message = msg.at(1);
            if (message->get_flag() == sio::message::flag_string) {
              that->OnReconnectionTicket(message->get_string());
            }
            RTC_LOG(LS_INFO) << "Reconnection success";
            //FIXME: 断网重连后可能造成，缓存断网前的offer信息，重连成功后，会发送缓存的offer，造成服务端出错。暂时不发送重连后暂时不发送缓存信息。并清空消息队列，
            //此处可能有未知的错误(需要考虑是否要丢弃重连前的所有信息)!!!
            // DrainQueuedMessages();
            that->DropQueuedMessagesNoCallback();
            that->TriggerOnServerReconnectionSuccess();
          });
    }
  });
  socket_client_->socket()->on(
      kEventNameStreamMessage,
      sio::socket::event_listener_aux(
          [weak_this](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            RTC_LOG(LS_VERBOSE) << "Received stream event.";
            auto that = weak_this.lock();
            if (!that) {
              return;
            }
            if (data->get_map()["status"] != nullptr &&
                data->get_map()["status"]->get_flag() == sio::message::flag_string &&
                data->get_map()["id"] != nullptr &&
                data->get_map()["id"]->get_flag() == sio::message::flag_string) {
              std::string stream_status = data->get_map()["status"]->get_string();
              std::string stream_id = data->get_map()["id"]->get_string();
              if (stream_status == "add") {
                auto stream_info = data->get_map()["data"];
                if (stream_info != nullptr && stream_info->get_flag() == sio::message::flag_object) {
                  std::lock_guard<std::mutex> lock(that->observer_mutex_);
                  for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
                    (*it)->OnStreamAdded(stream_info);
                  }
                }
              } else if (stream_status == "update") {
                sio::message::ptr update_message = sio::object_message::create();
                update_message->get_map()["id"] = sio::string_message::create(stream_id);
                auto stream_update = data->get_map()["data"];
                if (stream_update != nullptr && stream_update->get_flag() == sio::message::flag_object) {
                  update_message->get_map()["event"] = stream_update;
                }
                std::lock_guard<std::mutex> lock(that->observer_mutex_);
                for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
                  (*it)->OnStreamUpdated(update_message);
                }
              } else if (stream_status == "remove") {
                bool is_host = false;
                if (data->get_map()["isHost"]) {
                  is_host = data->get_map()["isHost"]->get_bool();
                }
                sio::message::ptr remove_message = sio::object_message::create();
                remove_message->get_map()["id"] = sio::string_message::create(stream_id);
                remove_message->get_map()["isHost"] = sio::bool_message::create(is_host);
                std::lock_guard<std::mutex> lock(that->observer_mutex_);
                for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
                  (*it)->OnStreamRemoved(remove_message);
                }
              }
            }
          }));
  socket_client_->socket()->on(
      kEventNameOnCustomMessage,
      sio::socket::event_listener_aux(
          [weak_this](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            RTC_LOG(LS_VERBOSE) << "Received custom message.";
            auto that = weak_this.lock();
            if (!that) {
              return;
            }
            std::string from = data->get_map()["from"]->get_string();
            std::string message = data->get_map()["message"]->get_string();
            std::string to = "me";
            auto target = data->get_map()["to"];
            if (target != nullptr && target->get_flag() == sio::message::flag_string) {
              to = target->get_string();
            }
            std::lock_guard<std::mutex> lock(that->observer_mutex_);
            for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
              (*it)->OnCustomMessage(from, message, to);
            }
          }));
  socket_client_->socket()->on(
      kEventNameOnUserPresence,
      sio::socket::event_listener_aux([weak_this](
          std::string const& name, sio::message::ptr const& data, bool is_ack,
          sio::message::list& ack_resp) {
        auto that = weak_this.lock();
        if (!that) {
          return;
        }
        RTC_LOG(LS_VERBOSE) << "Received user join/leave message.";
        if (data == nullptr || data->get_flag() != sio::message::flag_object ||
            data->get_map()["action"] == nullptr ||
            data->get_map()["action"]->get_flag() != sio::message::flag_string) {
          RTC_DCHECK(false);
          return;
        }
        auto participant_action = data->get_map()["action"]->get_string();
        if (participant_action == "join") {
          // Get the pariticipant ID from data;
          auto participant_info = data->get_map()["data"];
          if (participant_info != nullptr && participant_info->get_flag() == sio::message::flag_object
              && participant_info->get_map()["id"] != nullptr
              && participant_info->get_map()["id"]->get_flag() == sio::message::flag_string) {
            std::lock_guard<std::mutex> lock(that->observer_mutex_);
            for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
              (*it)->OnUserJoined(participant_info);
            }
          }
        } else if (participant_action == "leave") {
          auto participant_info = data->get_map()["data"];
          if (participant_info != nullptr && participant_info->get_flag() == sio::message::flag_string) {
            std::lock_guard<std::mutex> lock(that->observer_mutex_);
            for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
              (*it)->OnUserLeft(participant_info);
            }
          }
        } else {
          RTC_NOTREACHED();
        }
      }));
  socket_client_->socket()->on(
      kEventNameOnSignalingMessage,
      sio::socket::event_listener_aux(
          [weak_this](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            RTC_LOG(LS_VERBOSE) << "Received signaling message from erizo.";
            auto that = weak_this.lock();
            if (!that) {
              return;
            }
            std::lock_guard<std::mutex> lock(that->observer_mutex_);
            for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
              (*it)->OnSignalingMessage(data);
            }
          }));
  socket_client_->socket()->on(
      kEventNameOnDrop,
      sio::socket::event_listener_aux(
          [weak_this](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            RTC_LOG(LS_INFO) << "Received drop message.";
            auto that = weak_this.lock();
            if (!that) {
              return;
            }
            if (that->socket_client_) {
              that->socket_client_->set_reconnect_attempts(0);
            }
            std::lock_guard<std::mutex> lock(that->observer_mutex_);
            for (auto it = that->observers_.begin(); it != that->observers_.end(); ++it) {
              (*it)->OnServerDisconnected();
            }
          }));
  socket_client_->socket()->on(
      kEventNameConnectionFailed,
      sio::socket::event_listener_aux(
          [&](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            std::lock_guard<std::mutex> lock(observer_mutex_);
            for (auto it = observers_.begin(); it != observers_.end(); ++it) {
              (*it)->OnStreamError(data);
            }
          }));
  socket_client_->socket()->on(
      kEventNameSipAndPstnJoin,
      sio::socket::event_listener_aux(
          [&](std::string const& name, sio::message::ptr const& data,
              bool is_ack, sio::message::list& ack_resp) {
            std::lock_guard<std::mutex> lock(observer_mutex_);
            for (auto it = observers_.begin(); it != observers_.end(); ++it) {
              (*it)->OnSipAndPstnJoin(data);
            }
          }));
  // Store |on_failure| so it can be invoked if connect failed.
  connect_failure_callback_ = on_failure;
  socket_client_->connect(scheme.append(host));
}
void ConferenceSocketSignalingChannel::Disconnect(
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  if (!socket_client_->opened() && reconnection_attempted_ == 0 && on_failure) {
    // Socket.IO is not connected and not reconnecting.
    std::unique_ptr<Exception> e(new Exception(
        ExceptionType::kConferenceInvalidSession, "Socket.IO is not connected."));
    on_failure(std::move(e));
    // TODO: A corner case is execute Disconnect before first reconnection
    // attempt. So we don't return, still try to close socket.
  }
  reconnection_attempted_ = kReconnectionAttempts;
  disconnect_complete_ = on_success;
  if (socket_client_->opened()) {
    // Clear all pending failure callbacks after successful disconnect, don't check resp.
    std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
    socket_client_->socket()->emit(kEventNameLogout, nullptr,
                                   [weak_this](sio::message::list const& msg) {
                                    auto that = weak_this.lock();
                                    if (!that) {
                                      return;
                                    }
                                    that->DropQueuedMessages();
                                    that->socket_client_->close();
                                  });
  }
}

void ConferenceSocketSignalingChannel::CloseSocket() {
    // socket_client_->clear_con_listeners();

    // socket_client_->clear_socket_listeners();

    // socket_client_->close();
}

void ConferenceSocketSignalingChannel::SendSubscriptionUpdateMessage(
  sio::message::ptr options,
  std::function<void()> on_success,
  std::function<void(std::unique_ptr<Exception>)> on_failure) {
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
    shared_from_this();
  Emit(kEventNameSubscriptionControl, options,
    [weak_this, on_success, on_failure](sio::message::list const& msg) {
    if (auto that = weak_this.lock()) {
      that->OnEmitAck(msg, on_success, on_failure);
    }
  }, on_failure);
}
void ConferenceSocketSignalingChannel::SendInitializationMessage(
    sio::message::ptr options,
    std::string publish_stream_label,
    std::string subscribe_stream_label,
    std::function<void(std::string)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::list message_list;
  message_list.push(options);
  std::string event_name;
  if (publish_stream_label != "")
    event_name = kEventNamePublish;
  else if (subscribe_stream_label != "")
    event_name = kEventNameSubscribe;
  Emit(event_name, message_list,
       [=](sio::message::list const& msg) {
         RTC_LOG(LS_INFO) << "Received ack from server.";
         if (on_success == nullptr) {
           RTC_LOG(LS_WARNING) << "Does not implement success callback. Make sure "
                              "it is what you want.";
           return;
         }
         sio::message::ptr message = msg.at(0);
         if (message->get_flag() != sio::message::flag_string) {
           RTC_LOG(LS_WARNING)
               << "The first element of publish ack is not a string.";
           if (on_failure) {
             std::unique_ptr<Exception> e(new Exception(
                 ExceptionType::kConferenceInvalidParam,
                 "Received unkown message from server."));
             on_failure(std::move(e));
           }
           return;
         }
         if (message->get_string() == "ok") {
           if (msg.size() < 2) {
             RTC_LOG(LS_WARNING) << "msg 中的数据错误！！！";
             return;
           }
           if (msg.at(1)->get_flag() != sio::message::flag_object) {
             RTC_DCHECK(false);
             return;
           }
           std::string session_id = msg.at(1)->get_map()["id"]->get_string();
           if (event_name == kEventNamePublish || event_name == kEventNameSubscribe) {
             // Notify PeerConnectionChannel.
             on_success(session_id);
             return;
           }
           return;
         } else if (message->get_string() == "error" && msg.at(1) != nullptr &&
                    msg.at(1)->get_flag() == sio::message::flag_string) {
           if (on_failure) {
             std::unique_ptr<Exception> e(new Exception(
                 ExceptionType::kConferenceNotSupported, msg.at(1)->get_string()));
             on_failure(std::move(e));
           }
         } else {
           if (on_failure) {
             std::unique_ptr<Exception> e(new Exception(
                 ExceptionType::kConferenceInvalidParam,
                 "Ack for initializing message is not expected."));
             on_failure(std::move(e));
           }
           return;
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendSdp(
    sio::message::ptr message,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  sio::message::list message_list(message);
  // Add a null message for |to_to_deprecated|. Don't know its meaning.
  message_list.push(sio::null_message::create());
  Emit(kEventNameSignalingMessage, message_list,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendStreamEvent(
    const std::string& event,
    const std::string& stream_id,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr message = sio::object_message::create();
  message->get_map()["id"] = sio::string_message::create(stream_id);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit(event, message,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendCustomMessage(
    const std::string& message,
    const std::string& receiver,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr send_message = sio::object_message::create();
  if (receiver == "") {
    send_message->get_map()["to"] = sio::string_message::create("all");
  } else {
    send_message->get_map()["to"] = sio::string_message::create(receiver);
  }
  send_message->get_map()["message"] = sio::string_message::create(message);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit(kEventNameTextMessage, send_message,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendCommandMessage(
    const std::string& command,
    const std::string& message,
    std::function<void(sio::message::ptr receiveData)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr send_message = sio::object_message::create();
  send_message->get_map()["message"] = sio::string_message::create(message);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit(command, send_message,
    [weak_this, on_success, on_failure](sio::message::list const& msg) {
      auto that = weak_this.lock();
      if (that == nullptr) {
        if (on_failure != nullptr) {
          std::unique_ptr<Exception> e(new Exception(
                     ExceptionType::kConferenceInvalidParam,
                     "Signaling channel was destroyed before ack."));
                 on_failure(std::move(e));
        }
        return;
      }

      sio::message::ptr ack = msg.at(0);
      if (ack->get_flag() != sio::message::flag_string) {
        RTC_LOG(LS_WARNING) << "The first element of emit ack is not a string.";
        if (on_failure) {
          std::unique_ptr<Exception> e(
              new Exception(ExceptionType::kConferenceInvalidParam,
                            "Received unknown message from server."));
          on_failure(std::move(e));
        }
        return;
      }

      if (ack->get_string() == "success" || ack->get_string() == "ok") {
        if (on_success != nullptr) {
          std::thread t([on_success, ack] () {
            on_success(ack);
          });
          t.detach();
        }
      } else {
        RTC_LOG(LS_WARNING) << "Send message to MCU received negative ack.";
        if (msg.size() > 1) {
          sio::message::ptr error_ptr = msg.at(1);
          if (error_ptr->get_flag() == sio::message::flag_string) {
            RTC_LOG(LS_WARNING) << "Detail negative ack message: "
                            << error_ptr->get_string();
          }
        }
        if (on_failure != nullptr) {
          std::unique_ptr<Exception> e(
              new Exception(ExceptionType::kConferenceInvalidParam,
                            "Negative acknowledgment from server."));
          on_failure(std::move(e));
        }
      }
    },
    on_failure);
}
void ConferenceSocketSignalingChannel::SendStreamControlMessage(
    const std::string& stream_id,
    const std::string& action,
    const std::string& operation,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr payload = sio::object_message::create();
  payload->get_map()["id"] = sio::string_message::create(stream_id);
  payload->get_map()["operation"] = sio::string_message::create(operation);
  // Currently only pause/play will be processed here.
  if (operation == "pause" || operation == "play"
      || operation == "mix" || operation == "unmix") {
    //TODO(jianlin): Combine mute/unmute API with this.
    payload->get_map()["data"] = sio::string_message::create(action);
  }
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit(kEventNameStreamControl, payload,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendStreamsControlMessage(
    const std::vector<std::string>& streamIds,
    const std::string& value,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr payload = sio::object_message::create();

  sio::message::ptr ids = sio::array_message::create();
  for (auto it = streamIds.begin(); it != streamIds.end(); ++it) {
    sio::message::ptr streamId = sio::string_message::create((*it));
    ids->get_vector().push_back(streamId);
  }
  payload->get_map()["ids"] = ids;

  sio::message::ptr command = sio::object_message::create();
  command->get_map()["op"] = sio::string_message::create("replace");
  command->get_map()["path"] = sio::string_message::create("/media/audio/status");
  command->get_map()["value"] = sio::string_message::create(value);
  payload->get_map()["command"] = command;

  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit("controlStreams", payload,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::SendSubscriptionControlMessage(
    const std::string& stream_id,
    const std::string& action,
    const std::string& operation,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
    sio::message::ptr payload = sio::object_message::create();
    payload->get_map()["id"] = sio::string_message::create(stream_id);
    payload->get_map()["operation"] = sio::string_message::create(operation);
    // Currently only pause/play will be processed here.
    if (operation == "pause" || operation == "play") {
      payload->get_map()["data"] = sio::string_message::create(action);
    }
    std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
        shared_from_this();
    Emit(kEventNameSubscriptionControl, payload,
        [weak_this, on_success, on_failure](sio::message::list const& msg) {
        if (auto that = weak_this.lock()) {
            that->OnEmitAck(msg, on_success, on_failure);
        }
    },
        on_failure);
}
void ConferenceSocketSignalingChannel::Unsubscribe(
    const std::string& id,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr unsubscribe_message = sio::object_message::create();
  unsubscribe_message->get_map()["id"] = sio::string_message::create(id);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  Emit(kEventNameUnsubscribe, unsubscribe_message,
       [weak_this, on_success, on_failure](sio::message::list const& msg) {
         if (auto that = weak_this.lock()) {
           that->OnEmitAck(msg, on_success, on_failure);
         }
       },
       on_failure);
}
void ConferenceSocketSignalingChannel::OnEmitAck(
    sio::message::list const& msg,
    std::function<void()> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  sio::message::ptr ack = msg.at(0);
  if (ack->get_flag() != sio::message::flag_string) {
    RTC_LOG(LS_WARNING) << "The first element of emit ack is not a string.";
    if (on_failure) {
      std::unique_ptr<Exception> e(
          new Exception(ExceptionType::kConferenceInvalidParam,
                        "Received unknown message from server."));
      on_failure(std::move(e));
    }
    return;
  }
  if (ack->get_string() == "success" || ack->get_string() == "ok") {
    if (on_success != nullptr) {
      std::thread t(on_success);
      t.detach();
    }
  } else {
    RTC_LOG(LS_WARNING) << "Send message to MCU received negative ack.";
    if (msg.size() > 1) {
      sio::message::ptr error_ptr = msg.at(1);
      if (error_ptr->get_flag() == sio::message::flag_string) {
        RTC_LOG(LS_WARNING) << "Detail negative ack message: "
                        << error_ptr->get_string();
      }
    }
    if (on_failure != nullptr) {
      std::unique_ptr<Exception> e(
          new Exception(ExceptionType::kConferenceInvalidParam,
                        "Negative acknowledgment from server."));
      on_failure(std::move(e));
    }
  }
}
void ConferenceSocketSignalingChannel::OnReconnectionTicket(
    const std::string& ticket) {
  RTC_LOG(LS_VERBOSE) << "On reconnection ticket: " << ticket;
  reconnection_ticket_ = ticket;
  uint64_t now(0);
// rtc::TimeMillis() seems not work well on iOS. It returns half of the actual
// value on iPhone 6.
#if defined(WEBRTC_IOS)
  now = CFAbsoluteTimeGetCurrent();
  now += kMachLinuxTimeDelta;
  now *= 1000;
#else
  now = rtc::TimeMillis();
#endif
  std::string ticket_decoded(
      rtc::Base64::Decode(reconnection_ticket_, rtc::Base64::DO_STRICT));
  Json::Value ticket_json;
  Json::Reader ticket_reader;
  if (!ticket_reader.parse(ticket_decoded, ticket_json)) {
    RTC_NOTREACHED();
    RTC_LOG(LS_WARNING) << "Cannot parse reconnection ticket.";
    return;
  }
  Json::Value expiration_json;
  std::string expiration_str;
  if (rtc::GetValueFromJsonObject(ticket_json, "notAfter", &expiration_json)) {
    expiration_str = expiration_json.asString();
    auto expiration_time = std::stoll(expiration_str);
    int delay(expiration_time - now);
    if (delay < 0) {
      RTC_LOG(LS_WARNING)
          << "Reconnection ticket expiration time is earlier than now.";
      delay = 5 * 60 * 1000;  // Set delay to 5 mins.
    }
    RTC_LOG(LS_VERBOSE) << "Reconnection ticket will expire in: " << delay / 1000
                    << "seconds";
    std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
        shared_from_this();
    std::thread([weak_this, delay]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(delay));
      auto that = weak_this.lock();
      if (!that) {
        return;
      }
      that->RefreshReconnectionTicket();
    }).detach();
  }
}
void ConferenceSocketSignalingChannel::RefreshReconnectionTicket() {
  socket_client_->socket()->emit(
      kEventNameRefreshReconnectionTicket, nullptr,
      [=](sio::message::list const& ack) {
        if (ack.size() != 2) {
          RTC_NOTREACHED();
          RTC_LOG(LS_WARNING) << "Wired ack for refreshing reconnection ticket.";
          return;
        }
        std::string state = ack.at(0)->get_string();
        std::string message = ack.at(1)->get_string();
        if (state != "success" && state != "ok") {
          RTC_LOG(LS_WARNING) << "Refresh reconnection ticket failed. Error: "
                          << message;
          return;
        };
        OnReconnectionTicket(message);
      });
}
void ConferenceSocketSignalingChannel::TriggerOnServerDisconnected() {
  if (disconnect_complete_) {
    disconnect_complete_();
  }
  disconnect_complete_ = nullptr;
  if (observers_.size() > 0) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    for (auto it = observers_.begin(); it != observers_.end(); ++it) {
      (*it)->OnServerDisconnected();
    }
  }
}

void ConferenceSocketSignalingChannel::TriggerOnServerReconnecting() {
  if (observers_.size() > 0) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    for (auto it = observers_.begin(); it != observers_.end(); ++it) {
      (*it)->OnServerReconnecting();
    }
  }
}

void ConferenceSocketSignalingChannel::TriggerOnServerReconnectionSuccess() {
  if (observers_.size() > 0) {
    std::lock_guard<std::mutex> lock(observer_mutex_);
    for (auto it = observers_.begin(); it != observers_.end(); ++it) {
      (*it)->OnServerReconnectionSuccess();
    }
  }
}

void ConferenceSocketSignalingChannel::RequestConferenceInfo(
    std::function<void(sio::message::ptr room_info)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  if (!socket_client_->opened() && reconnection_attempted_ == 0 && on_failure) {
    // Socket.IO is not connected and not reconnecting.
    std::unique_ptr<Exception> e(new Exception(
        ExceptionType::kConferenceInvalidSession, "Socket.IO is not connected."));
    on_failure(std::move(e));
    return;
  }
  if (socket_client_->opened()) {
    sio::message::ptr jsonObject = sio::object_message::create();
    jsonObject->get_map()["id"] = sio::string_message::create(participant_id_);
    socket_client_->socket()->emit("room-infos", jsonObject, [=](sio::message::list const& msg) {
      sio::message::ptr message = msg.at(1);
      auto room_info = message->get_map()["room"];
      if (room_info == nullptr ||
          room_info->get_flag() != sio::message::flag_object) {
        std::unique_ptr<Exception> e(new Exception(
            ExceptionType::kConferenceInvalidSession, "无效的room info."));
        on_failure(std::move(e));
        return;
      }

      if (on_success) {
        on_success(message);
      }
    });
  }
}

void ConferenceSocketSignalingChannel::RequestParticipantsList(
    std::function<void(sio::message::ptr participantsList)> on_success,
    std::function<void(std::unique_ptr<Exception>)> on_failure) {
  if (!socket_client_->opened() && reconnection_attempted_ == 0 && on_failure) {
    // Socket.IO is not connected and not reconnecting.
    std::unique_ptr<Exception> e(new Exception(
        ExceptionType::kConferenceInvalidSession, "Socket.IO is not connected."));
    on_failure(std::move(e));
    return;
  }

  sio::message::ptr requestParam = sio::string_message::create("string");
  socket_client_->socket()->emit("getParticipantsList", requestParam, [=](sio::message::list const& msg) {
    if (msg.size() <= 0 && on_failure) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidSession, "空消息！！！"));
      on_failure(std::move(e));
      return;
    }
    sio::message::ptr participantsListStr = msg.at(0);
    if ((participantsListStr == nullptr ||
        participantsListStr->get_flag() != sio::message::flag_string) && on_failure) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidSession, "消息类型错误！！！"));
      on_failure(std::move(e));
      return;
    }
    if (on_success) {
      on_success(participantsListStr);
    }
  });
}

void ConferenceSocketSignalingChannel::TriggerOnServerUpdateConferenceInfoSuccess() {
  sio::message::ptr jsonObject = sio::object_message::create();
  jsonObject->get_map()["id"] = sio::string_message::create(participant_id_);
  socket_client_->socket()->emit("room-infos", jsonObject, [=](sio::message::list const& msg) {
    sio::message::ptr message = msg.at(1);
    auto room_info = message->get_map()["room"];
    if (room_info == nullptr ||
        room_info->get_flag() != sio::message::flag_object) {
      RTC_LOG(LS_ERROR) << "无效的room info";
      return;
    }

    std::lock_guard<std::mutex> lock(observer_mutex_);
    for (auto it = observers_.begin(); it != observers_.end(); ++it) {
      (*it)->OnServerUpdateConferenceInfoSuccess(message);
    }
  });
}

void ConferenceSocketSignalingChannel::Emit(
    const std::string& name,
    const sio::message::list& message,
    const std::function<void(sio::message::list const&)> ack,
    const std::function<void(std::unique_ptr<Exception>)>
        on_failure) {
  int message_id(0);
  {
    std::lock_guard<std::mutex> lock(outgoing_message_mutex_);
    message_id = outgoing_message_id_++;
  }
#if 0
  std::string sio_name = "signaling";
  sio::message::ptr request_name = sio::string_message::create(name);
  sio::message::list new_message(message);
  new_message.insert(0, request_name);
#endif
  // SioMessage sio_message(message_id, sio_name, new_message, ack, on_failure);
  SioMessage sio_message(message_id, name, message, ack, on_failure);
  outgoing_messages_.push(sio_message);
  std::weak_ptr<ConferenceSocketSignalingChannel> weak_this =
      shared_from_this();
  socket_client_->socket()->emit(
      name, message, [weak_this, message_id](sio::message::list const& msg) {
        RTC_LOG(LS_INFO) << "Received ack for message ID: " << message_id;
        auto that = weak_this.lock();
        if (!that) {
          RTC_LOG(LS_WARNING) << "Signaling channel was destroyed before ack.";
          return;
        }
        std::function<void(sio::message::list const&)> callback(nullptr);
        {
          std::lock_guard<std::mutex> lock(that->outgoing_message_mutex_);
          if (that->outgoing_messages_.empty()) {
            //RTC_NOTREACHED();
            return;
          }
          /*RTC_DCHECK_EQ(message_id, that->outgoing_messages_.front().id)
              << "Unordered Socket.IO message.";*/
          while (that->outgoing_messages_.front().id < message_id) {
            RTC_LOG(LS_WARNING) << "Potential unordered Socket.IO message.";
            that->outgoing_messages_.pop();
          }
          if (that->outgoing_messages_.front().id > message_id) {
            RTC_LOG(LS_ERROR) << "Original message for " << message_id
                          << " is not found.";
          }
          callback = that->outgoing_messages_.front().ack;
          that->outgoing_messages_.pop();
        }
        if (callback) {
          callback(msg);
        }
      });
}
//重连成功后丢弃消息队列
void ConferenceSocketSignalingChannel::DropQueuedMessagesNoCallback() {
  std::lock_guard<std::mutex> lock(outgoing_message_mutex_);
  while (!outgoing_messages_.empty()) {
    outgoing_messages_.pop();
  }
}
void ConferenceSocketSignalingChannel::DropQueuedMessages() {
  // TODO(jianjunz): Trigger on_failure in another thread. In current
  // implementation, failure callback MUST NOT acquire
  // |outgoing_message_mutex_|. Otherwise, deadlock may happen.
  std::lock_guard<std::mutex> lock(outgoing_message_mutex_);
  while (!outgoing_messages_.empty()) {
    if (outgoing_messages_.front().on_failure != nullptr) {
      std::unique_ptr<Exception> e(new Exception(
          ExceptionType::kConferenceInvalidSession,
          "Failed to delivery message."));
      outgoing_messages_.front().on_failure(std::move(e));
    }
    outgoing_messages_.pop();
  }
}
void ConferenceSocketSignalingChannel::DrainQueuedMessages() {
  std::queue<SioMessage> temp_queue;
  {
    std::lock_guard<std::mutex> lock(outgoing_message_mutex_);
    std::swap(temp_queue, outgoing_messages_);
  }
  RTC_LOG(LS_INFO) << "outgoing_messages_ number after swap: "
               << outgoing_messages_.size();
  while (!temp_queue.empty()) {
    auto sio_message = temp_queue.front();
    // MUST release |outgoing_message_mutex_| before Emit because Emit acquires
    // mutex.
    Emit(sio_message.name, sio_message.message, sio_message.ack,
         sio_message.on_failure);
    temp_queue.pop();
  }
}
sio::message::ptr ConferenceSocketSignalingChannel::ResolutionMessage(
    const owt::base::Resolution& resolution) {
  sio::message::ptr resolution_message = sio::object_message::create();
  // Width and height could be double instead of int.
  resolution_message->get_map()["width"] =
      sio::int_message::create(resolution.width);
  resolution_message->get_map()["height"] =
      sio::int_message::create(resolution.height);
  return resolution_message;
}
}
}
