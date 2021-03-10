// OWTConferenceSignalingChannelProtocol.h
//
//
#import <Foundation/Foundation.h>
#import <WebRTC/RTCMacros.h>
NS_ASSUME_NONNULL_BEGIN
RTC_OBJC_EXPORT
@protocol OWTConferenceSignalingChannelProtocol<NSObject>

- (void)connect:(NSString*)token
      onSuccess:(void (^)(NSString*))onSuccess
      onFailure:(void (^)(NSError*))onFailure;

@end
NS_ASSUME_NONNULL_END
