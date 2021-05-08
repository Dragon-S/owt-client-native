//
//  SocketIoClientInterface.h
//
//  Created by longlong.shi on 2021/4/28.
//

#ifndef SocketIoClientInterface_h
#define SocketIoClientInterface_h

#include <string>
#include <functional>

#include "talk/owt/include/sio_message.h"

namespace sio {
class SocketIoClientInterface {
public:
    enum close_reason {
        close_reason_normal,
        close_reason_drop
    };
    
    typedef std::function<void(void)> con_listener;
    typedef std::function<void(close_reason const& reason)> close_listener;
    typedef std::function<void(unsigned, unsigned)> reconnect_listener;
    typedef std::function<void(std::string const& nsp)> socket_listener;
    
    typedef std::function<void(const std::string& name,message::ptr const& message,bool need_ack, message::list& ack_message)> event_listener_aux;
    
    //set listeners and event bindings.
    virtual void set_open_listener(con_listener const& l) = 0;
    virtual void set_fail_listener(con_listener const& l) = 0;
    virtual void set_reconnecting_listener(con_listener const& l) = 0;
    virtual void set_reconnect_listener(reconnect_listener const& l) = 0;
    virtual void set_close_listener(close_listener const& l) = 0;
    virtual void set_socket_open_listener(socket_listener const& l) = 0;
    virtual void set_socket_close_listener(socket_listener const& l) = 0;
    
    virtual void clear_con_listeners() = 0;
    virtual void clear_socket_listeners() = 0;
    
    virtual void setup(const std::string& uri) = 0;
    virtual void connect() = 0;
    
    virtual void emit(std::string const& name,
                      message::list const& msglist = nullptr,
                      std::function<void (message::list const&)> const& ack = nullptr) = 0;
    
    virtual void on(std::string const& event_name,
                    event_listener_aux const& func) = 0;
    
    virtual void set_reconnect_attempts(int attempts) = 0;
    virtual void set_reconnect_delay(unsigned int millis) = 0;
    virtual void set_reconnect_delay_max(unsigned int millis) = 0;
    
    virtual void close() = 0;
    
    virtual bool opened() = 0;
};
}
#endif /* SocketIoClientInterface_h */
