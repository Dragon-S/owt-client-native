// ConferenceSocketIoSignalingChannelObjcImpl.mm
//
//
#include "talk/owt/sdk/conference/objc/ConferenceSocketIoSignalingChannelObjcImpl.h"
#include "webrtc/rtc_base/logging.h"
namespace owt {
namespace conference {
ConferenceSocketIoSignalingChannelObjcImpl::ConferenceSocketIoSignalingChannelObjcImpl(
    id<OWTConferenceSignalingChannelProtocol> signaling_channel)
    : signaling_channel_(signaling_channel),
      reconnection_ticket_(""),
      participant_id_(""),
      reconnection_attempted_(0),
      is_reconnection_(false),
      outgoing_message_id_(1) {
  RTC_LOG(LS_INFO) << "ConferenceSocketIoSignalingChannelObjcImpl Constructor";
}

ConferenceSocketIoSignalingChannelObjcImpl::~ConferenceSocketIoSignalingChannelObjcImpl() {
  RTC_LOG(LS_INFO) << "ConferenceSocketIoSignalingChannelObjcImpl Destructor";
}

void ConferenceSocketIoSignalingChannelObjcImpl::AddObserver(
    ConferenceSignalingChannelObserverInterface& observer) {
  std::lock_guard<std::mutex> lock(observer_mutex_);
  if (std::find(observers_.begin(), observers_.end(), &observer) !=
      observers_.end()) {
    RTC_LOG(LS_INFO) << "Adding duplicated observer.";
    return;
  }
  observers_.push_back(&observer);
}

void ConferenceSocketIoSignalingChannelObjcImpl::RemoveObserver(
    ConferenceSignalingChannelObserverInterface& observer) {
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

  //初始化socketio

  //设置关闭socket回调

  //设置socket连接失败回调

  //设置socket重连回调

  //设置socket连接成功回调

  //

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

}
}
