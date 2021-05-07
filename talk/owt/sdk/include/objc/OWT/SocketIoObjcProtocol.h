//
//  SocketIoObjcProtocol.h
//  SocketioTest
//
//  Created by longlong.shi on 2021/4/27.
//

#import <Foundation/Foundation.h>

typedef void (^ConnectSuccessCallback)(void);
typedef void (^ConnectFailedCallback)(NSError*_Nullable);
typedef void (^ConnectClosedCallback)(void);
typedef void (^ReconnectCallback)(void);
typedef void (^ReconnectingCallback)(void);
typedef void (^DisconnectCallback)(void);

NS_ASSUME_NONNULL_BEGIN

@protocol SocketIoObjcProtocol <NSObject>

- (void)setConnectSuccessCallback:(ConnectSuccessCallback)successCallback;
- (void)setConnectFailedCallback:(ConnectFailedCallback)failedCallback;
- (void)setConnectClosedCallback:(ConnectClosedCallback)closeCallback;
- (void)setReconnectCallback:(ReconnectCallback)reconnectCallback;
- (void)setReconnectingCallback:(ReconnectingCallback)reconnectingCallback;
- (void)setDisconnectCallback:(DisconnectCallback)disconnectCallback;

- (void)clearConnectListeners;
- (void)clearSocketListeners;

- (void)setupSocket:(NSString *)uri;

- (void)connect;

- (void)emit:(NSString*)name
     message:(NSArray*)message
    callback:(void (^)(NSArray * _Nonnull))callback;

- (void)addListenerWithEventName:(NSString*)eventName
                        callback:(void (^)(NSArray*))callback;

- (void)close;

- (BOOL)opened;

- (void)setReconnectAttempts:(int)attempts;
- (void)setReconnectDelay:(unsigned int)millis;
- (void)setReconnectDelayMax:(unsigned int)millis;

@end

NS_ASSUME_NONNULL_END
