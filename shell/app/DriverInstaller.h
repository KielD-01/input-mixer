#pragma once

#include <juce_core/juce_core.h>

namespace DriverInstaller
{
    /** True on macOS when in-app install (Homebrew or official pkg download) is supported. */
    bool isDriverPackageAvailable();

    /** True when a BlackHole HAL plug-in is present on disk (may still need CoreAudio restart). */
    bool isBlackHoleHalOnDisk();

    /** Official BlackHole download page for manual install fallback. */
    juce::String blackHoleDownloadPageUrl();

    bool restartCoreAudioServices(juce::String& message);

    bool installVirtualMicDriver(juce::String& errorMessage);
}
