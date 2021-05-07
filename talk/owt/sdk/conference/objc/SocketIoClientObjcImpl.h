//
//  SocketIoClientObjcImpl.h
//  SocketioTest
//
//  Created by longlong.shi on 2021/4/28.
//

#ifndef SocketIoClientObjcImpl_h
#define SocketIoClientObjcImpl_h

#include "talk/owt/sdk/include/cpp/owt/conference/SocketIoClientInterface.h"

#import "talk/owt/sdk/include/objc/OWT/SocketIoObjcProtocol.h"

namespace sio {

class SocketIoClientObjcImpl : public SocketIoClientInterface {
public:
    SocketIoClientObjcImpl(id<SocketIoObjcProtocol> socketio);
    virtual ~SocketIoClientObjcImpl();
    
    //set listeners and event bindings.
    virtual void set_open_listener(con_listener const& l) override;
    virtual void set_fail_listener(con_listener const& l) override;
    virtual void set_reconnecting_listener(con_listener const& l) override;
    virtual void set_reconnect_listener(reconnect_listener const& l) override;
    virtual void set_close_listener(close_listener const& l) override;
    virtual void set_socket_open_listener(socket_listener const& l) override;
    virtual void set_socket_close_listener(socket_listener const& l) override;
    
    virtual void clear_con_listeners() override;
    virtual void clear_socket_listeners() override;
    
    virtual void setupSocket(const std::string& uri) override;
    virtual void connect() override;
    
    virtual void emit(std::string const& name,
                      message::list const& msglist = nullptr,
                      std::function<void (message::list const&)> const& ack = nullptr) override;
    
    virtual void on(std::string const& event_name,
                    event_listener_aux const& func) override;
    
    virtual void set_reconnect_attempts(int attempts) override;
    virtual void set_reconnect_delay(unsigned int millis) override;
    virtual void set_reconnect_delay_max(unsigned int millis) override;
    
    virtual void close() override;
    
    virtual bool opened() override;
    
private:
    id<SocketIoObjcProtocol> m_socketio_objc;
    
    con_listener m_open_listener;
    con_listener m_fail_listener;
    con_listener m_reconnecting_listener;
    reconnect_listener m_reconnect_listener;
    close_listener m_close_listener;
    
    socket_listener m_socket_open_listener;
    socket_listener m_socket_close_listener;
    
    //disable copy constructor and assign operator.
    SocketIoClientObjcImpl(SocketIoClientObjcImpl const&) {}
    void operator=(SocketIoClientObjcImpl const&) {}
};

}

#endif //SocketIoClientObjcImpl
