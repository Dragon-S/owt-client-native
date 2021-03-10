// conferencesocketiosignalingchannel.h
//
//
#ifndef OWT_CONFERENCE_SOCKETIOSIGNALINGCHANNEL_H_
#define OWT_CONFERENCE_SOCKETIOSIGNALINGCHANNEL_H_
#include "talk/owt/sdk/conference/conferencesignalingchannelinterface.h"
namespace owt {
namespace conference {
class ConferenceSocketIoSignalingChannel : public ConferenceSignalingChannelInterface
    : public std::enable_shared_from_this<ConferenceSocketIoSignalingChannel> {
 public:
  explicit ConferenceSocketIoSignalingChannel();
  virtual ~ConferenceSocketIoSignalingChannel();

  virtual void AddObserver(ConferenceSignalingChannelObserverInterface& observer);
  virtual void RemoveObserver(ConferenceSignalingChannelObserverInterface& observer);
  virtual void Connect(
      const std::string& token,
      std::function<void(Json::Value& room_info,
                         std::vector<const conference::User> users)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendInitializationMessage(
      sio::message::ptr options,
      std::string publish_stream_label,
      std::string subcribe_stream_label,
      std::function<void(std::string)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendSdp(
      Json::Value& options,
      std::string& sdp,
      bool is_publish,
      std::function<void(Json::Value& ack, std::string& stream_id)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendStreamEvent(
      const std::string& event,
      const std::string& stream_id,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendCustomMessage(
      const std::string& message,
      const std::string& receiver,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendCommandMessage(
      const std::string& command,
      const std::string& message,
      std::function<void(sio::message::ptr receiveData)> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendStreamControlMessage(
      const std::string& stream_id,
      const std::string& action,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendSubscriptionControlMessage(
      const std::string& stream_id,
      const std::string& action,
      const std::string& operation,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void SendSubscriptionUpdateMessage(
      sio::message::ptr options,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void Unsubscribe(
      const std::string& id,
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
  virtual void Disconnect(
      std::function<void()> on_success,
      std::function<void(std::unique_ptr<Exception>)> on_failure);
};
}
}
#endif  // OWT_CONFERENCE_SOCKETIOSIGNALINGCHANNEL_H_
