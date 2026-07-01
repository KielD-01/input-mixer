#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Security/Security.h>
#import <dispatch/dispatch.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static NSString *const kScreenCapturePromptedKey = @"InputMixerScreenCapturePrompted";
static NSString *const kMicrophonePromptedKey = @"InputMixerMicrophonePrompted";

static BOOL g_microphone_request_attempted = NO;
static BOOL g_microphone_prompted_persisted = NO;
static int g_adhoc_build = -1;

static BOOL input_mixer_microphone_was_prompted(void) {
    if (g_microphone_prompted_persisted) {
        return YES;
    }
    return [[NSUserDefaults standardUserDefaults] boolForKey:kMicrophonePromptedKey];
}

static void input_mixer_mark_microphone_prompted(void) {
    g_microphone_prompted_persisted = YES;
    [[NSUserDefaults standardUserDefaults] setBool:YES forKey:kMicrophonePromptedKey];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

static BOOL input_mixer_screen_capture_was_prompted(void) {
    return [[NSUserDefaults standardUserDefaults] boolForKey:kScreenCapturePromptedKey];
}

static BOOL input_mixer_is_adhoc_build(void) {
    if (g_adhoc_build >= 0) {
        return g_adhoc_build ? YES : NO;
    }

    SecCodeRef selfCode = NULL;
    if (SecCodeCopySelf(kSecCSDefaultFlags, &selfCode) != errSecSuccess) {
        g_adhoc_build = 0;
        return NO;
    }

    CFDictionaryRef signingInfo = NULL;
    if (SecCodeCopySigningInformation(selfCode, kSecCSSigningInformation, &signingInfo) != errSecSuccess) {
        CFRelease(selfCode);
        g_adhoc_build = 0;
        return NO;
    }

    CFNumberRef flagsNum = (CFNumberRef)CFDictionaryGetValue(signingInfo, kSecCodeInfoFlags);
    uint32_t flags = 0;
    if (flagsNum != NULL) {
        CFNumberGetValue(flagsNum, kCFNumberSInt32Type, &flags);
    }

    BOOL adhoc = (flags & kSecCodeSignatureAdhoc) != 0;
    CFRelease(signingInfo);
    CFRelease(selfCode);
    g_adhoc_build = adhoc ? 1 : 0;
    return adhoc;
}

// Never call getShareableContent or SCStream unless preflight is true — both trigger the TCC dialog.
static BOOL input_mixer_has_screen_capture_access_internal(void) {
    if (@available(macOS 13.0, *)) {
        return CGPreflightScreenCaptureAccess();
    }
    return NO;
}

static void input_mixer_fetch_shareable_content(
    void (^handler)(SCShareableContent *content, NSError *error)) API_AVAILABLE(macos(13.0))
{
    if (!input_mixer_has_screen_capture_access_internal()) {
        handler(nil, [NSError errorWithDomain:@"InputMixer"
                                         code:-1
                                     userInfo:@{NSLocalizedDescriptionKey: @"Screen capture not authorized"}]);
        return;
    }

    if (@available(macOS 15.0, *)) {
        [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                                     onScreenWindowsOnly:NO
                                                       completionHandler:handler];
    } else {
        [SCShareableContent getShareableContentWithCompletionHandler:handler];
    }
}

static SCStream *g_stream = nil;
static dispatch_queue_t g_audio_queue = nil;

typedef void (*AudioCallback)(const float *data, size_t len, void *ctx);
static AudioCallback g_callback = NULL;
static void *g_callback_ctx = NULL;

@interface InputMixerStreamOutput : NSObject <SCStreamOutput>
@end

@implementation InputMixerStreamOutput
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
    if (type != SCStreamOutputTypeAudio || g_callback == NULL) return;

    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sampleBuffer);
    if (format == NULL) return;

    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(format);
    if (asbd == NULL) return;

    CMBlockBufferRef blockBuffer = NULL;
    char stackBuf[8192];
    AudioBufferList *bufferList = (AudioBufferList *)stackBuf;

    OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
        sampleBuffer, NULL, bufferList, sizeof(stackBuf), NULL, NULL, 0, &blockBuffer);

    if (status != noErr) return;

    const UInt32 numBuffers = bufferList->mNumberBuffers;
    if (numBuffers == 0) {
        if (blockBuffer) CFRelease(blockBuffer);
        return;
    }

    if (numBuffers == 1) {
        AudioBuffer buf = bufferList->mBuffers[0];
        size_t floatCount = buf.mDataByteSize / sizeof(float);
        if (floatCount > 0 && buf.mData != NULL) {
            g_callback((const float *)buf.mData, floatCount, g_callback_ctx);
        }
    } else {
        // Non-interleaved stereo: merge into interleaved L/R for the mixer.
        const float *left = (const float *)bufferList->mBuffers[0].mData;
        const float *right = (numBuffers > 1) ? (const float *)bufferList->mBuffers[1].mData : left;
        size_t leftCount = bufferList->mBuffers[0].mDataByteSize / sizeof(float);
        size_t rightCount = (numBuffers > 1)
            ? bufferList->mBuffers[1].mDataByteSize / sizeof(float)
            : leftCount;
        size_t frameCount = leftCount < rightCount ? leftCount : rightCount;
        if (frameCount == 0 || left == NULL || right == NULL) {
            if (blockBuffer) CFRelease(blockBuffer);
            return;
        }
        float interleaved[4096];
        size_t maxFrames = sizeof(interleaved) / (2 * sizeof(float));
        size_t offset = 0;
        while (offset < frameCount) {
            size_t chunk = frameCount - offset;
            if (chunk > maxFrames) chunk = maxFrames;
            for (size_t i = 0; i < chunk; i++) {
                interleaved[i * 2] = left[offset + i];
                interleaved[i * 2 + 1] = right[offset + i];
            }
            g_callback(interleaved, chunk * 2, g_callback_ctx);
            offset += chunk;
        }
    }

    if (blockBuffer) CFRelease(blockBuffer);
}
@end

static void free_c_strings(char **arr, size_t count) {
    if (arr == NULL) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

extern "C" {

void input_mixer_sck_stop_capture(void);

int input_mixer_has_microphone_access(void) {
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    return status == AVAuthorizationStatusAuthorized ? 1 : 0;
}

int input_mixer_is_microphone_access_denied(void) {
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    return (status == AVAuthorizationStatusDenied
            || status == AVAuthorizationStatusRestricted) ? 1 : 0;
}

int input_mixer_should_request_microphone_access(void) {
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status != AVAuthorizationStatusNotDetermined) {
        return 0;
    }
    if (input_mixer_microphone_was_prompted()) {
        return 0;
    }
    return 1;
}

void input_mixer_set_microphone_prompted(int prompted) {
    if (prompted) {
        input_mixer_mark_microphone_prompted();
    }
}

int input_mixer_request_microphone_access(void) {
    AVAuthorizationStatus status =
        [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusAuthorized) {
        return 1;
    }
    if (status == AVAuthorizationStatusDenied || status == AVAuthorizationStatusRestricted) {
        return 0;
    }
    if (status != AVAuthorizationStatusNotDetermined) {
        return 0;
    }
    if (g_microphone_request_attempted) {
        return 0;
    }
    if (!input_mixer_should_request_microphone_access()) {
        return 0;
    }
    g_microphone_request_attempted = YES;
    // Mark prompted before the dialog so dismissal (Allow or Don't Allow) never re-triggers.
    input_mixer_mark_microphone_prompted();

    __block int granted = 0;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                             completionHandler:^(BOOL allowed) {
        granted = allowed ? 1 : 0;
        dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    return granted;
}

int input_mixer_sck_has_screen_capture_access(void) {
    return input_mixer_has_screen_capture_access_internal() ? 1 : 0;
}

// 0 = granted, 1 = not determined, 2 = prompted but needs relaunch/re-grant, 3 = adhoc rebuild needs re-grant
int input_mixer_sck_screen_capture_access_state(void) {
    if (@available(macOS 13.0, *)) {
        if (CGPreflightScreenCaptureAccess()) {
            return 0;
        }
        if (input_mixer_is_adhoc_build()) {
            return 3;
        }
        if (!input_mixer_screen_capture_was_prompted()) {
            return 1;
        }
        return 2;
    }
    return 1;
}

void input_mixer_sck_refresh_screen_capture_access(void) {
    // No-op: access is derived from CGPreflightScreenCaptureAccess only.
}

int input_mixer_sck_is_adhoc_build(void) {
    return input_mixer_is_adhoc_build() ? 1 : 0;
}

int input_mixer_should_request_screen_capture_access(void) {
    if (@available(macOS 13.0, *)) {
        if (CGPreflightScreenCaptureAccess()) {
            return 0;
        }
        if ([[NSUserDefaults standardUserDefaults] boolForKey:kScreenCapturePromptedKey]) {
            return 0;
        }
        return 1;
    }
    return 0;
}

int input_mixer_sck_request_screen_capture_access(void) {
    // Screen Recording is granted only via System Settings — never CGRequestScreenCaptureAccess.
    if (@available(macOS 13.0, *)) {
        return CGPreflightScreenCaptureAccess() ? 1 : 0;
    }
    return 0;
}

static BOOL input_mixer_is_self_app(NSString *bundleId) {
    return bundleId != nil && [bundleId isEqualToString:@"com.inputmixer.app"];
}

static BOOL input_mixer_bundle_matches(NSString *candidate, NSString *target) {
    if (candidate == nil || target == nil || target.length == 0) return NO;
    if ([candidate isEqualToString:target]) return YES;
    NSString *candidateSlug = [[candidate lowercaseString] stringByReplacingOccurrencesOfString:@"." withString:@""];
    NSString *targetSlug = [[target lowercaseString] stringByReplacingOccurrencesOfString:@"." withString:@""];
    return [candidateSlug isEqualToString:targetSlug];
}

static SCWindow *input_mixer_best_window_for_bundle(SCShareableContent *content, NSString *targetBundle) {
    SCWindow *best = nil;
    CGFloat bestArea = 0;
    for (SCWindow *win in content.windows) {
        if (win.owningApplication == nil) continue;
        if (!input_mixer_bundle_matches(win.owningApplication.bundleIdentifier, targetBundle)) continue;
        if (win.frame.size.width < 1.0 || win.frame.size.height < 1.0) continue;
        CGFloat area = win.frame.size.width * win.frame.size.height;
        if (area > bestArea) {
            bestArea = area;
            best = win;
        }
    }
    return best;
}

static void input_mixer_build_source_arrays(
    SCShareableContent *content,
    NSMutableArray<NSString *> *names,
    NSMutableArray<NSString *> *bundles,
    NSMutableArray<NSString *> *subtitles,
    NSMutableArray<NSNumber *> *windowIds) {

    NSMutableSet<NSString *> *bundlesWithWindows = [NSMutableSet set];

    for (SCWindow *win in content.windows) {
        if (win.owningApplication == nil) continue;
        NSString *bundleId = win.owningApplication.bundleIdentifier;
        if (input_mixer_is_self_app(bundleId)) continue;
        if (win.frame.size.width < 1.0 || win.frame.size.height < 1.0) continue;

        NSString *title = [win.title stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (title.length == 0) continue;

        if (bundleId != nil) {
            [bundlesWithWindows addObject:bundleId];
        }

        [names addObject:win.owningApplication.applicationName ?: @"Window"];
        [bundles addObject:bundleId ?: @""];
        [subtitles addObject:win.title ?: @""];
        [windowIds addObject:@(win.windowID)];
    }

    for (SCRunningApplication *app in content.applications) {
        if (app.bundleIdentifier == nil) continue;
        if (input_mixer_is_self_app(app.bundleIdentifier)) continue;
        if ([bundlesWithWindows containsObject:app.bundleIdentifier]) continue;

        [names addObject:app.applicationName ?: app.bundleIdentifier];
        [bundles addObject:app.bundleIdentifier];
        [subtitles addObject:@""];
        [windowIds addObject:@(0)];
    }
}

int input_mixer_sck_enumerate_apps(
    char ***out_names,
    char ***out_bundle_ids,
    char ***out_subtitles,
    uint64_t **out_window_ids,
    size_t *out_count) {

    if (@available(macOS 13.0, *)) {
        if (!input_mixer_has_screen_capture_access_internal()) {
            return -1;
        }

        __block int result = -1;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        // Avoid blocking the main run loop while waiting for SCK (UI thread calls this).
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            input_mixer_fetch_shareable_content(^(SCShareableContent *content, NSError *error) {
                if (error || content == nil) {
                    result = -1;
                    dispatch_semaphore_signal(sem);
                    return;
                }

                NSMutableArray<NSString *> *names = [NSMutableArray array];
                NSMutableArray<NSString *> *bundles = [NSMutableArray array];
                NSMutableArray<NSString *> *subtitles = [NSMutableArray array];
                NSMutableArray<NSNumber *> *windowIds = [NSMutableArray array];

                input_mixer_build_source_arrays(content, names, bundles, subtitles, windowIds);

                if (names.count == 0) {
                    result = -1;
                    dispatch_semaphore_signal(sem);
                    return;
                }

                size_t count = names.count;
                char **nameArr = (char **)calloc(count, sizeof(char *));
                char **bundleArr = (char **)calloc(count, sizeof(char *));
                char **subtitleArr = (char **)calloc(count, sizeof(char *));
                uint64_t *winArr = (uint64_t *)calloc(count, sizeof(uint64_t));

                for (size_t i = 0; i < count; i++) {
                    nameArr[i] = strdup([names[i] UTF8String]);
                    bundleArr[i] = strdup([bundles[i] UTF8String]);
                    subtitleArr[i] = strdup([subtitles[i] UTF8String]);
                    winArr[i] = [windowIds[i] unsignedLongLongValue];
                }

                *out_names = nameArr;
                *out_bundle_ids = bundleArr;
                *out_subtitles = subtitleArr;
                *out_window_ids = winArr;
                *out_count = count;
                result = 0;
                dispatch_semaphore_signal(sem);
            });
        });

        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        return result;
    }
    return -1;
}

int input_mixer_sck_start_capture(
    const char *bundle_id,
    uint64_t window_id,
    AudioCallback callback,
    void *ctx) {

    if (@available(macOS 13.0, *)) {
        if (!input_mixer_has_screen_capture_access_internal()) {
            fprintf(stderr, "[InputMixer] SCK start blocked: Screen Recording not authorized\n");
            return -1;
        }

        if (g_stream != nil) {
            input_mixer_sck_stop_capture();
        }

        g_callback = callback;
        g_callback_ctx = ctx;

        __block int result = -1;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
            input_mixer_fetch_shareable_content(^(SCShareableContent *content, NSError *error) {
                if (error || content == nil) {
                    fprintf(stderr, "[InputMixer] SCK shareable content failed: %s\n",
                            error ? [[error localizedDescription] UTF8String] : "no content");
                    result = -1;
                    dispatch_semaphore_signal(sem);
                    return;
                }

                NSString *targetBundle = [NSString stringWithUTF8String:bundle_id];
                SCContentFilter *filter = nil;
                SCWindow *selectedWindow = nil;

                if (window_id != 0) {
                    for (SCWindow *win in content.windows) {
                        if (win.windowID == window_id) {
                            selectedWindow = win;
                            filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:win];
                            break;
                        }
                    }
                    if (filter == nil) {
                        fprintf(stderr,
                                "[InputMixer] Window id %llu not found for bundle %s\n",
                                window_id, bundle_id);
                    }
                }

                if (filter == nil) {
                    selectedWindow = input_mixer_best_window_for_bundle(content, targetBundle);
                    if (selectedWindow != nil) {
                        filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:selectedWindow];
                        fprintf(stderr,
                                "[InputMixer] Auto-selected window \"%s\" (id=%u) for %s\n",
                                [selectedWindow.title UTF8String],
                                selectedWindow.windowID,
                                bundle_id);
                    }
                }

                if (filter == nil) {
                    for (SCRunningApplication *app in content.applications) {
                        if (input_mixer_bundle_matches(app.bundleIdentifier, targetBundle)) {
                            SCDisplay *display = content.displays.firstObject;
                            if (display == nil) {
                                fprintf(stderr, "[InputMixer] No display available for app capture\n");
                                break;
                            }
                            filter = [[SCContentFilter alloc] initWithDisplay:display
                                                          includingApplications:@[app]
                                                          exceptingWindows:@[]];
                            fprintf(stderr,
                                    "[InputMixer] Using app-wide capture for %s (%s)\n",
                                    [app.applicationName UTF8String],
                                    [app.bundleIdentifier UTF8String]);
                            break;
                        }
                    }
                }

                if (filter == nil) {
                    fprintf(stderr,
                            "[InputMixer] No capture target for bundle %s (window_id=%llu)\n",
                            bundle_id, window_id);
                    result = -1;
                    dispatch_semaphore_signal(sem);
                    return;
                }

                SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
                config.capturesAudio = YES;
                config.excludesCurrentProcessAudio = YES;
                config.sampleRate = 48000;
                config.channelCount = 2;
                config.width = 2;
                config.height = 2;

                g_stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:nil];
                InputMixerStreamOutput *output = [[InputMixerStreamOutput alloc] init];
                g_audio_queue = dispatch_queue_create("input_mixer.sck.audio", DISPATCH_QUEUE_SERIAL);

                NSError *outputError = nil;
                BOOL added = [g_stream addStreamOutput:output
                                                  type:SCStreamOutputTypeAudio
                                     sampleHandlerQueue:g_audio_queue
                                                  error:&outputError];
                if (!added) {
                    fprintf(stderr,
                            "[InputMixer] addStreamOutput failed: %s\n",
                            outputError ? [[outputError localizedDescription] UTF8String] : "unknown");
                    g_stream = nil;
                    result = -1;
                    dispatch_semaphore_signal(sem);
                    return;
                }

                [g_stream startCaptureWithCompletionHandler:^(NSError * _Nullable err) {
                    if (err) {
                        fprintf(stderr,
                                "[InputMixer] startCapture failed: %s\n",
                                [[err localizedDescription] UTF8String]);
                        g_stream = nil;
                        result = -1;
                    } else {
                        fprintf(stderr, "[InputMixer] SCK audio capture started for %s\n", bundle_id);
                        result = 0;
                    }
                    dispatch_semaphore_signal(sem);
                }];
            });
        });

        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        return result;
    }
    return -1;
}

void input_mixer_sck_stop_capture(void) {
    if (g_stream != nil) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [g_stream stopCaptureWithCompletionHandler:^(NSError * _Nullable error) {
            dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        g_stream = nil;
    }
    g_callback = NULL;
    g_callback_ctx = NULL;
}

} // extern "C"
