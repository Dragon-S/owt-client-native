//
//  OWTConferenceSignalingChannelProtocol.h
//
//  Created by longlong.shi on 2021/3/12.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^ConnectSuccessCallback)(void);
typedef void (^ConnectFailedCallback)(NSError*);
typedef void (^ConnectClosedCallback)(void);
typedef void (^ReconnectCallback)(void);
typedef void (^DisconnectCallback)(void);

const NSString* kReconnectAttemptsKeyName = @"reconnectAttempts";
const NSString* kReconnectWaitKeyName = @"reconnectWait";
const NSString* kReconnectWaitMaxKeyName = @"reconnectWaitMax";
const NSString* kHostUrlKeyName = @"hostUrl";

@protocol OWTConferenceSignalingChannelProtocol <NSObject>

- (void)setup:(NSDictionary*)config;

- (void)connectOnSuccess:(void (^)())onSuccess
               onFailure:(void (^)(NSError * _Nonnull))onFailure;

- (void)closeSocket;

- (bool)socketOpened;

- (void)emit:(NSString*)name
     message:(NSArray*)message
    callback:(void (^)(NSArray * _Nonnull))callback;

- (void)addListenerWithEventName:(NSString*)eventName
                        callback:(void (^)(NSArray*))callback;

@end

NS_ASSUME_NONNULL_END
