#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>

#include <stdlib.h>
#include <string.h>

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

static BOOL deviceHasChannelsInScope(AudioObjectID deviceID, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        scope,
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
    const BOOL hasChannels = (err == noErr && list->mNumberBuffers > 0);
    free(list);
    return hasChannels;
}

static void free_c_strings(char **arr, size_t count)
{
    if (arr == NULL)
        return;
    for (size_t i = 0; i < count; i++)
        free(arr[i]);
    free(arr);
}

typedef BOOL (*DeviceFilter)(AudioObjectID deviceID);

static int enumerate_devices(
    DeviceFilter filter,
    char ***out_names,
    char ***out_uids,
    size_t *out_count)
{
    *out_names = NULL;
    *out_uids = NULL;
    *out_count = 0;

    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr)
        return -1;

    const int count = (int)(size / sizeof(AudioObjectID));
    AudioObjectID *devices = (AudioObjectID *)malloc(size);
    if (devices == NULL)
        return -1;

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, devices) != noErr) {
        free(devices);
        return -1;
    }

    NSMutableArray<NSString *> *names = [NSMutableArray array];
    NSMutableArray<NSString *> *uids = [NSMutableArray array];

    for (int i = 0; i < count; i++) {
        if (!filter(devices[i]))
            continue;

        CFStringRef nameRef = NULL;
        CFStringRef uidRef = NULL;
        if (getDeviceName(devices[i], &nameRef) != noErr || nameRef == NULL)
            continue;
        if (getDeviceUID(devices[i], &uidRef) != noErr || uidRef == NULL) {
            CFRelease(nameRef);
            continue;
        }

        [names addObject:(__bridge NSString *)nameRef];
        [uids addObject:(__bridge NSString *)uidRef];
        CFRelease(nameRef);
        CFRelease(uidRef);
    }

    free(devices);

    if (names.count == 0)
        return 0;

    const size_t n = names.count;
    char **nameArr = (char **)calloc(n, sizeof(char *));
    char **uidArr = (char **)calloc(n, sizeof(char *));
    if (nameArr == NULL || uidArr == NULL) {
        free(nameArr);
        free(uidArr);
        return -1;
    }

    for (size_t i = 0; i < n; i++) {
        nameArr[i] = strdup([names[i] UTF8String]);
        uidArr[i] = strdup([uids[i] UTF8String]);
        if (nameArr[i] == NULL || uidArr[i] == NULL) {
            free_c_strings(nameArr, n);
            free_c_strings(uidArr, n);
            return -1;
        }
    }

    *out_names = nameArr;
    *out_uids = uidArr;
    *out_count = n;
    return 0;
}

static BOOL isInputDevice(AudioObjectID deviceID)
{
    return deviceHasChannelsInScope(deviceID, kAudioDevicePropertyScopeInput);
}

static BOOL isOutputDevice(AudioObjectID deviceID)
{
    return deviceHasChannelsInScope(deviceID, kAudioDevicePropertyScopeOutput);
}

extern "C" {

int input_mixer_hal_enumerate_input_devices(
    char ***out_names,
    char ***out_uids,
    size_t *out_count)
{
    return enumerate_devices(isInputDevice, out_names, out_uids, out_count);
}

int input_mixer_hal_enumerate_output_devices(
    char ***out_names,
    char ***out_uids,
    size_t *out_count)
{
    return enumerate_devices(isOutputDevice, out_names, out_uids, out_count);
}

void input_mixer_hal_free_device_list(char **arr, size_t count)
{
    free_c_strings(arr, count);
}

void input_mixer_hal_free_string(char *s)
{
    free(s);
}

int input_mixer_hal_input_name_for_uid(const char *uid, char **out_name)
{
    if (uid == NULL || out_name == NULL)
        return -1;
    *out_name = NULL;

    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &size) != noErr)
        return -1;

    const int count = (int)(size / sizeof(AudioObjectID));
    AudioObjectID *devices = (AudioObjectID *)malloc(size);
    if (devices == NULL)
        return -1;

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, devices) != noErr) {
        free(devices);
        return -1;
    }

    int result = -1;
    for (int i = 0; i < count; i++) {
        if (!isInputDevice(devices[i]))
            continue;

        CFStringRef uidRef = NULL;
        if (getDeviceUID(devices[i], &uidRef) != noErr || uidRef == NULL)
            continue;

        const char *deviceUid = [(__bridge NSString *)uidRef UTF8String];
        if (deviceUid != NULL && strcmp(deviceUid, uid) == 0) {
            CFStringRef nameRef = NULL;
            if (getDeviceName(devices[i], &nameRef) == noErr && nameRef != NULL) {
                *out_name = strdup([(__bridge NSString *)nameRef UTF8String]);
                result = (*out_name != NULL) ? 0 : -1;
            }
            CFRelease(nameRef);
            CFRelease(uidRef);
            break;
        }
        CFRelease(uidRef);
    }

    free(devices);
    return result;
}

} // extern "C"
