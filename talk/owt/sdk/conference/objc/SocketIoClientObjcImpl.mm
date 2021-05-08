//
//  SocketIoClientObjcImpl.m
//  SocketioTest
//
//  Created by longlong.shi on 2021/4/28.
//

#include "talk/owt/sdk/conference/objc/SocketIoClientObjcImpl.h"

static NSDictionary* sio_object_message_2_ns_dict(sio::message::ptr message);
static sio::message::ptr ns_dict_2_sio_object_message(NSDictionary* message);

static NSNumber* sio_bool_message_2_ns_number(sio::message::ptr message) {
    assert(message);
    
    bool message_bool = message->get_bool();
    return [NSNumber numberWithBool:message_bool];
}

static NSNumber* sio_double_message_2_ns_number(sio::message::ptr message) {
    assert(message);
    
    double message_double = message->get_double();
    return [NSNumber numberWithDouble:message_double];
}

static NSNumber* sio_integer_message_2_ns_number(sio::message::ptr message) {
    assert(message);
    
    int64_t message_int = message->get_int();
    return [NSNumber numberWithLongLong:message_int];
}

static NSString* sio_string_message_2_ns_string(sio::message::ptr message) {
    assert(message);
    
    std::string message_str = message->get_string();
    return [NSString stringWithCString:message_str.c_str() encoding:[NSString defaultCStringEncoding]];
}

static NSArray* sio_array_message_2_ns_array(sio::message::ptr message) {
    assert(message);
    
    NSMutableArray* ns_array_message = [[NSMutableArray alloc] init];
    std::vector<sio::message::ptr>& vector = message->get_vector();
    std::vector<sio::message::ptr>::iterator iter;
    for (iter = vector.begin(); iter != vector.end(); iter++) {
        if ((*iter)->get_flag() == sio::message::flag_object) {
            NSDictionary* ns_object_message = sio_object_message_2_ns_dict(*iter);
            [ns_array_message addObject:ns_object_message];
        } else if ((*iter)->get_flag() == sio::message::flag_array) {
            NSArray *ns_sub_array_message = sio_array_message_2_ns_array(*iter);
            [ns_array_message addObject:ns_sub_array_message];
        } else if ((*iter)->get_flag() == sio::message::flag_string) {
            NSString* ns_string_message = sio_string_message_2_ns_string(*iter);
            [ns_array_message addObject:ns_string_message];
        } else if ((*iter)->get_flag() == sio::message::flag_double) {
            NSNumber* ns_double_message = sio_double_message_2_ns_number(*iter);
            [ns_array_message addObject:ns_double_message];
        } else if ((*iter)->get_flag() == sio::message::flag_integer) {
            NSNumber* ns_integer_message = sio_integer_message_2_ns_number(*iter);
            [ns_array_message addObject:ns_integer_message];
        } else if ((*iter)->get_flag() == sio::message::flag_boolean) {
            NSNumber* ns_bool_message = sio_bool_message_2_ns_number(*iter);
            [ns_array_message addObject:ns_bool_message];
        }
    }
    
    return ns_array_message;
}

static NSDictionary* sio_object_message_2_ns_dict(sio::message::ptr message) {
    assert(message);
    
    NSMutableDictionary* ns_object_message = [[NSMutableDictionary alloc] init];
    std::map<std::string, sio::message::ptr>& map = message->get_map();
    std::map<std::string, sio::message::ptr>::iterator iter;
    for (iter = map.begin(); iter != map.end(); iter++) {
        NSString* ns_key = [NSString stringWithCString:iter->first.c_str()
                                              encoding:[NSString defaultCStringEncoding]];
        sio::message::ptr value = iter->second;
        if (value->get_flag() == sio::message::flag_object) {
            NSDictionary* ns_sub_object_message = sio_object_message_2_ns_dict(value);
            [ns_object_message setObject:ns_sub_object_message forKey:ns_key];
        } else if (value->get_flag() == sio::message::flag_array) {
            NSArray* ns_array_message = sio_array_message_2_ns_array(value);
            [ns_object_message setObject:ns_array_message forKey:ns_key];
        } else if (value->get_flag() == sio::message::flag_string) {
            NSString* ns_string_message = sio_string_message_2_ns_string(value);
            [ns_object_message setObject:ns_string_message forKey:ns_key];
        } else if (value->get_flag() == sio::message::flag_double) {
            NSNumber* ns_duble_message = sio_double_message_2_ns_number(value);
            [ns_object_message setObject:ns_duble_message forKey:ns_key];
        } else if (value->get_flag() == sio::message::flag_boolean) {
            NSNumber* ns_bool_message = sio_bool_message_2_ns_number(value);
            [ns_object_message setObject:ns_bool_message forKey:ns_key];
        } else if (value->get_flag() == sio::message::flag_integer) {
            NSNumber* ns_integer_message = sio_integer_message_2_ns_number(value);
            [ns_object_message setObject:ns_integer_message forKey:ns_key];
        }
    }
    
    return ns_object_message;
}

static sio::message::ptr ns_number_2_sio_message(NSNumber* message) {
    assert(message);
    
    NSString* ns_objc_type = [NSString stringWithFormat:@"%s", [message objCType]];
    NSString* int64_base_type = [NSString stringWithFormat:@"%s", @encode(int64_t)];
    NSString* bool_base_type = [NSString stringWithFormat:@"%s", @encode(bool)];
    NSString* double_base_type = [NSString stringWithFormat:@"%s", @encode(double)];
    
    if (NSOrderedSame == [ns_objc_type compare:int64_base_type options:NSCaseInsensitiveSearch]) {
        return sio::int_message::create([message longLongValue]);
    } else if (NSOrderedSame == [ns_objc_type compare:double_base_type options:NSCaseInsensitiveSearch]) {
        return sio::double_message::create([message doubleValue]);
    } else if (NSOrderedSame == [ns_objc_type compare:bool_base_type options:NSCaseInsensitiveSearch]) {
        return sio::double_message::create([message boolValue]);
    }
    
    return nullptr;
}

static sio::message::ptr ns_string_2_sio_string_message(NSString* message) {
    assert(message);
    
    return sio::string_message::create([message UTF8String]);
}

static sio::message::ptr ns_array_2_sio_array_message(NSArray* message) {
    assert(message);
//    assert(message.count);//TODO:layout字段对应的数组是空的，此处暂时不做检测
    
    sio::message::ptr sio_message = sio::array_message::create();
    
    for (id item in message) {
        if ([item isKindOfClass:[NSDictionary class]]) {
            sio::message::ptr sio_sub_message = ns_dict_2_sio_object_message(item);
            sio_message->get_vector().push_back(sio_sub_message);
        } else if ([item isKindOfClass:[NSArray class]]) {
            sio::message::ptr sio_sub_message = ns_array_2_sio_array_message(item);
            sio_message->get_vector().push_back(sio_sub_message);
        } else if ([item isKindOfClass:[NSString class]]) {
            sio_message->get_vector().push_back(ns_string_2_sio_string_message(item));
        } else if ([item isKindOfClass:[NSNumber class]]) {
            sio_message->get_vector().push_back(ns_number_2_sio_message(item));
        }
    }
    
    return sio_message;
}

static sio::message::ptr ns_dict_2_sio_object_message(NSDictionary* message) {
    assert(message);
    assert([message allKeys]);
    assert([message allKeys].count);
    
    sio::message::ptr sio_message = sio::object_message::create();
    
    [message enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        const char* c_str_key = [key UTF8String];
        if ([obj isKindOfClass:[NSDictionary class]]) {
            sio::message::ptr sio_sub_message = ns_dict_2_sio_object_message(obj);
            sio_message->get_map()[c_str_key] = sio_sub_message;
        } else if ([obj isKindOfClass:[NSArray class]]) {
            sio::message::ptr sio_sub_message = ns_array_2_sio_array_message(obj);
            sio_message->get_map()[c_str_key] = sio_sub_message;
        } else if ([obj isKindOfClass:[NSString class]]) {
            sio_message->get_map()[c_str_key] = ns_string_2_sio_string_message(obj);
        } else if ([obj isKindOfClass:[NSNumber class]]) {
            sio_message->get_map()[c_str_key] = ns_number_2_sio_message(obj);
        }
    }];
    
    return sio_message;
}

static const sio::message::ptr& get_message_with_list(sio::message::list const& message_list) {
    if(message_list.size() > 0) {
        return message_list[0];
    } else {
        static sio::message::ptr null_ptr;
        return null_ptr;
    }
}

namespace sio {
SocketIoClientObjcImpl::SocketIoClientObjcImpl(id<SocketIoObjcProtocol> socketio) {
    m_socketio_objc = socketio;
}

SocketIoClientObjcImpl::~SocketIoClientObjcImpl() {
    if (m_socketio_objc) {
        m_socketio_objc = nil;
    }
}

void SocketIoClientObjcImpl::set_open_listener(con_listener const& l) {
    m_open_listener = l;
    [m_socketio_objc setConnectSuccessCallback:^{
         if (m_open_listener) {
            m_open_listener();
         }
     }];
}

void SocketIoClientObjcImpl::set_fail_listener(con_listener const& l) {
    m_fail_listener = l;
    [m_socketio_objc setConnectFailedCallback:^(NSError * _Nullable error) {
         if (m_fail_listener) {
            m_fail_listener();
         }
     }];
}

void SocketIoClientObjcImpl::set_close_listener(close_listener const& l) {
    m_close_listener = l;
    [m_socketio_objc setConnectClosedCallback:^{
         if (m_close_listener) {
            m_close_listener(close_reason_normal);
         }
     }];
}

void SocketIoClientObjcImpl::set_reconnect_listener(reconnect_listener const& l) {
    m_reconnect_listener = l;
    [m_socketio_objc setReconnectCallback:^{
         if (m_reconnect_listener) {
            m_reconnect_listener(1, 1);
         }
     }];
}

void SocketIoClientObjcImpl::set_reconnecting_listener(con_listener const& l) {
    m_reconnecting_listener = l;
    [m_socketio_objc setReconnectingCallback:^{
         if (m_reconnecting_listener) {
            m_reconnecting_listener();
         }
     }];
}

void SocketIoClientObjcImpl::set_socket_open_listener(socket_listener const& l) {
    m_socket_open_listener = l;
}

void SocketIoClientObjcImpl::set_socket_close_listener(socket_listener const& l) {
    m_socket_close_listener = l;
}

void SocketIoClientObjcImpl::clear_con_listeners() {
    m_open_listener = nullptr;
    m_fail_listener = nullptr;
    m_reconnecting_listener = nullptr;
    m_reconnect_listener = nullptr;
    m_close_listener = nullptr;
}

void SocketIoClientObjcImpl::clear_socket_listeners() {
    m_socket_open_listener = nullptr;
    m_socket_close_listener = nullptr;
}

void SocketIoClientObjcImpl::setup(const std::string& uri) {
    NSString *ns_str = [NSString stringWithCString:uri.c_str()
                                          encoding:[NSString defaultCStringEncoding]];
    [m_socketio_objc setup:ns_str];
}

void SocketIoClientObjcImpl::connect() {
    [m_socketio_objc connect];
}

void SocketIoClientObjcImpl::emit(std::string const& name,
                                  message::list const& msglist,
                                  std::function<void (message::list const&)> const& ack) {
    
  __block std::function<void (message::list const&)> callback = ack;
  NSMutableArray* ns_array_message = [[NSMutableArray alloc] init];
  for (int i = 0; i < (int)msglist.size(); i++) {
      message::ptr item = msglist.at(i);
      if (item->get_flag() == message::flag_object) {
          NSDictionary* ns_sub_object_message = sio_object_message_2_ns_dict(item);
          [ns_array_message addObject:ns_sub_object_message];
      } else if (item->get_flag() == message::flag_array) {
          NSArray* ns_sub_array_message = sio_array_message_2_ns_array(item);
          [ns_array_message addObject:ns_sub_array_message];
      } else if (item->get_flag() == message::flag_string) {
          NSString* ns_string_message = sio_string_message_2_ns_string(item);
          [ns_array_message addObject:ns_string_message];
      } else if (item->get_flag() == message::flag_double) {
          NSNumber* ns_double_message = sio_double_message_2_ns_number(item);
          [ns_array_message addObject:ns_double_message];
      } else if (item->get_flag() == message::flag_boolean) {
          NSNumber* ns_boolean_message = sio_bool_message_2_ns_number(item);
          [ns_array_message addObject:ns_boolean_message];
      } else if (item->get_flag() == message::flag_integer) {
          NSNumber* ns_integer_message = sio_integer_message_2_ns_number(item);
          [ns_array_message addObject:ns_integer_message];
      }
  }
  
  NSString* ns_name = [NSString stringWithCString:name.c_str() encoding:[NSString defaultCStringEncoding]];
  [m_socketio_objc emit:ns_name message:ns_array_message callback:^(NSArray * _Nonnull message) {
      message::list message_list;
      for (id item in message) {
          if ([item isKindOfClass:[NSDictionary class]]) {
              sio::message::ptr sio_sub_message = ns_dict_2_sio_object_message(item);
              message_list.push(sio_sub_message);
          } else if ([item isKindOfClass:[NSArray class]]) {
              sio::message::ptr sio_sub_message = ns_array_2_sio_array_message(item);
              message_list.push(sio_sub_message);
          } else if ([item isKindOfClass:[NSString class]]) {
              message_list.push(ns_string_2_sio_string_message(item));
          } else if ([item isKindOfClass:[NSNumber class]]) {
              message_list.push(ns_number_2_sio_message(item));
          }
      }
      callback(message_list);
   }];
}

void SocketIoClientObjcImpl::on(std::string const& event_name,
                                event_listener_aux const& func) {
    __block event_listener_aux callback = func;
    NSString* ns_event_name = [NSString stringWithCString:event_name.c_str() encoding:[NSString defaultCStringEncoding]];
    [m_socketio_objc addListenerWithEventName:ns_event_name callback:^(NSArray * _Nonnull message) {
        message::list message_list;
        for (id item in message) {
            if ([item isKindOfClass:[NSDictionary class]]) {
                sio::message::ptr sio_sub_message = ns_dict_2_sio_object_message(item);
                message_list.push(sio_sub_message);
            } else if ([item isKindOfClass:[NSArray class]]) {
                sio::message::ptr sio_sub_message = ns_array_2_sio_array_message(item);
                message_list.push(sio_sub_message);
            } else if ([item isKindOfClass:[NSString class]]) {
                message_list.push(ns_string_2_sio_string_message(item));
            } else if ([item isKindOfClass:[NSNumber class]]) {
                message_list.push(ns_number_2_sio_message(item));
            }
        }
        
        message::list ack_message;
        callback(event_name, get_message_with_list(message_list), false, ack_message);
     }];
}

void SocketIoClientObjcImpl::close() {
    [m_socketio_objc close];
}

bool SocketIoClientObjcImpl::opened() {
    return [m_socketio_objc opened];
}

void SocketIoClientObjcImpl::set_reconnect_attempts(int attempts) {
    [m_socketio_objc setReconnectAttempts:attempts];
}

void SocketIoClientObjcImpl::set_reconnect_delay(unsigned int millis) {
    [m_socketio_objc setReconnectDelay:millis];
}

void SocketIoClientObjcImpl::set_reconnect_delay_max(unsigned int millis) {
    [m_socketio_objc setReconnectDelayMax:millis];
}

} //sio
