// conferencesignalingchannelinterface.h
//
//
#ifndef OWT_CONFERENCE_SIGNALINGCHANNELINTERFACE_H_
#define OWT_CONFERENCE_SIGNALINGCHANNELINTERFACE_H_
#include <functional>
#include <memory>
#include "talk/owt/sdk/include/cpp/owt/conference/conferenceuser.h"
#include "webrtc/rtc_base/json.h"
namespace owt {
namespace conference {
class ConferenceSignalingChannelInterface {
 public:
  virtual void AddObserver(ConferenceSignalingChannelObserverInterface& observer) = 0;
  virtual void RemoveObserver(ConferenceSignalingChannelObserverInterface& observer) = 0;
  virtual void Connect(
      const std::string& token,
      std::function<void(Json::Value& room_info,
                         std::vector<const conference::User> users)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendInitializationMessage(
      sio::message::ptr options,
      std::string publish_stream_label,
      std::string subcribe_stream_label,
      std::function<void(std::string)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendSdp(
      Json::Value& options,
      std::string& sdp,
      bool is_publish,
      std::function<void(Json::Value& ack, std::string& stream_id)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendStreamEvent(
      const std::string& event,
      const std::string& stream_id,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendCustomMessage(
      const std::string& message,
      const std::string& receiver,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendCommandMessage(
      const std::string& command,
      const std::string& message,
      std::function<void(sio::message::ptr receiveData)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendStreamControlMessage(
      const std::string& stream_id,
      const std::string& action,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendSubscriptionControlMessage(
      const std::string& stream_id,
      const std::string& action,
      const std::string& operation,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void SendSubscriptionUpdateMessage(
      sio::message::ptr options,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void Unsubscribe(
      const std::string& id,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
  virtual void Disconnect(
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure) = 0;
};
}
}
#endif  // OWT_CONFERENCE_SIGNALINGCHANNELINTERFACE_H_
