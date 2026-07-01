#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

static NSString *const kAggregateUID = @"audio.inputmixer.channel";
static NSString *const kAggregateName = @"Input Mixer Channel";

static OSStatus getDeviceUID(AudioObjectID deviceID, CFStringRef *uidOut)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef uid = NULL;
    UInt32 size = sizeof(uid);
    OSStatus err = AudioObjectGetPropertyData(deviceID, &address, 0, NULL, &size, &uid);
    if (err == noErr && uidOut != NULL)
        *uidOut = uid;
    return err;
}

static OSStatus getDeviceName(AudioObjectID deviceID, CFStringRef *nameOut)
{
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef name = NULL;
    UInt32 size = sizeof(name);
    OSStatus err = AudioObjectGetPropertyData(deviceID, &address, 0, NULL, &size, &name);
    if (err == noErr && nameOut != NULL)
        *nameOut = name;
    return err;
}

static BOOL deviceHasInputChannels(AudioObjectID deviceID)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(deviceID, &address, 0, NULL, &size);
    if (err != noErr || size == 0)
        return NO;

    AudioBufferList *list = (AudioBufferList *)malloc(size);
    if (list == NULL)
        return NO;

    err = AudioObjectGetPropertyData(deviceID, &address, 0, NULL, &size, list);
    const BOOL hasInput = (err == noErr && list->mNumberBuffers > 0);
    free(list);
    return hasInput;
}

static AudioObjectID findBlackHoleInputDevice(void)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr)
        return kAudioObjectUnknown;

    const int count = (int)(size / sizeof(AudioObjectID));
    AudioObjectID *devices = (AudioObjectID *)malloc(size);
    if (devices == NULL)
        return kAudioObjectUnknown;

    AudioObjectID best = kAudioObjectUnknown;
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, devices);

    for (int i = 0; i < count; i++)
    {
        CFStringRef name = NULL;
        if (getDeviceName(devices[i], &name) != noErr || name == NULL)
            continue;

        NSString *nameStr = (__bridge NSString *)name;
        if (![nameStr localizedCaseInsensitiveContainsString:@"BlackHole"])
        {
            CFRelease(name);
            continue;
        }
        if (!deviceHasInputChannels(devices[i]))
        {
            CFRelease(name);
            continue;
        }

        if ([nameStr localizedCaseInsensitiveContainsString:@"2ch"])
        {
            best = devices[i];
            CFRelease(name);
            break;
        }

        if (best == kAudioObjectUnknown)
            best = devices[i];

        CFRelease(name);
    }

    free(devices);
    return best;
}

static AudioObjectID deviceForUID(NSString *uid)
{
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyTranslateUIDToDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef uidRef = (__bridge CFStringRef)uid;
    AudioValueTranslation translation;
    translation.mInputData = &uidRef;
    translation.mInputDataSize = sizeof(CFStringRef);
    AudioObjectID deviceID = kAudioObjectUnknown;
    translation.mOutputData = &deviceID;
    translation.mOutputDataSize = sizeof(AudioObjectID);
    UInt32 size = sizeof(translation);
    OSStatus err = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &address, 0, NULL, &size, &translation);
    return (err == noErr) ? deviceID : kAudioObjectUnknown;
}

extern "C" {

int input_mixer_ensure_channel_aggregate(void)
{
    if (deviceForUID(kAggregateUID) != kAudioObjectUnknown)
        return 1;

    const AudioObjectID blackHole = findBlackHoleInputDevice();
    if (blackHole == kAudioObjectUnknown)
        return 0;

    CFStringRef bhUID = NULL;
    if (getDeviceUID(blackHole, &bhUID) != noErr || bhUID == NULL)
        return 0;

    NSDictionary *inputSub = @{
        @kAudioSubDeviceUIDKey : (__bridge NSString *)bhUID,
        @kAudioSubDeviceDriftCompensationKey : @YES,
    };
    NSDictionary *desc = @{
        @kAudioAggregateDeviceNameKey : kAggregateName,
        @kAudioAggregateDeviceUIDKey : kAggregateUID,
        @kAudioAggregateDeviceSubDeviceListKey : @[ inputSub ],
        @kAudioAggregateDeviceMasterSubDeviceKey : (__bridge NSString *)bhUID,
        @kAudioAggregateDeviceClockDeviceKey : (__bridge NSString *)bhUID,
    };

    CFRelease(bhUID);

    AudioObjectID aggregateID = kAudioObjectUnknown;
    const OSStatus err = AudioHardwareCreateAggregateDevice((__bridge CFDictionaryRef)desc, &aggregateID);
    return (err == noErr && aggregateID != kAudioObjectUnknown) ? 1 : 0;
}

} // extern "C"
