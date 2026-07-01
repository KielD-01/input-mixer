#include "DriverInstaller.h"

#if JUCE_MAC
#include "bridge/RustBridge.h"

namespace
{
constexpr const char* kBlackHolePkgUrl =
    "https://existential.audio/downloads/BlackHole2ch-0.7.0.pkg";
constexpr const char* kBlackHoleDownloadPage = "https://existential.audio/blackhole/";

juce::String escapeForAppleScript(const juce::String& path)
{
    juce::String escaped;
    escaped.preallocateBytes(path.getNumBytesAsUTF8() + 8);

    for (auto c : path)
    {
        if (c == '\\' || c == '"')
            escaped << '\\';

        escaped << c;
    }

    return escaped;
}

bool runElevatedShellScript(const juce::String& shellCommand, juce::String& message, int timeoutMs)
{
    const auto script = "do shell script \"" + escapeForAppleScript(shellCommand) + "\" with administrator privileges";

    juce::ChildProcess proc;
    juce::StringArray args;
    args.add("osascript");
    args.add("-e");
    args.add(script);

    if (! proc.start(args))
    {
        message = "Could not start the installer.";
        return false;
    }

    if (! proc.waitForProcessToFinish(timeoutMs))
    {
        message = "Installation timed out. Try again.";
        return false;
    }

    message = proc.readAllProcessOutput().trim();
    const auto exitCode = proc.getExitCode();

    if (exitCode != 0)
    {
        if (message.containsIgnoreCase("User canceled") || message.containsIgnoreCase("canceled"))
            message = "Installation was cancelled.";
        else if (message.isEmpty())
            message = "Installation failed.";

        return false;
    }

    return true;
}

bool runShellCommand(const juce::String& shellCommand, juce::String& output, int timeoutMs)
{
    juce::ChildProcess proc;

    if (! proc.start(shellCommand))
        return false;

    if (! proc.waitForProcessToFinish(timeoutMs))
        return false;

    output = proc.readAllProcessOutput().trim();
    return proc.getExitCode() == 0;
}

bool blackHoleHalExistsAt(const char* path)
{
    return juce::File(path).exists();
}

bool restartCoreAudio(juce::String& warningMessage)
{
    if (runElevatedShellScript("launchctl kickstart -kp system/com.apple.audio.coreaudiod",
                               warningMessage,
                               60000))
        return true;

    juce::String killWarning;

    if (runElevatedShellScript("/usr/bin/killall coreaudiod", killWarning, 60000))
        return true;

    if (killWarning.isNotEmpty())
        warningMessage = killWarning;

    return false;
}

bool downloadBlackHolePkg(const juce::File& destination, juce::String& errorMessage)
{
    if (destination.existsAsFile())
        destination.deleteFile();

    const auto downloadCmd = "/usr/bin/curl -fsSL -o \""
                             + destination.getFullPathName()
                             + "\" \""
                             + juce::String(kBlackHolePkgUrl)
                             + "\"";

    juce::String output;

    if (! runShellCommand(downloadCmd, output, 300000))
    {
        if (output.isNotEmpty())
            errorMessage = "Could not download the virtual microphone driver: " + output;
        else
            errorMessage = "Could not download the virtual microphone driver. "
                           "Check your internet connection and try again.";

        return false;
    }

    if (! destination.existsAsFile() || destination.getSize() < 1024)
    {
        errorMessage = "The downloaded installer file looks invalid. Try again later.";
        return false;
    }

    return true;
}

bool installViaDownloadedPkg(juce::String& errorMessage, bool& needsReboot)
{
    needsReboot = false;

    const auto tempPkg = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("InputMixer-BlackHole2ch.pkg");

    if (! downloadBlackHolePkg(tempPkg, errorMessage))
        return false;

    const auto installCommand = "/usr/sbin/installer -pkg \"" + tempPkg.getFullPathName() + "\" -target / 2>&1";
    juce::String installOutput;

    if (! runElevatedShellScript(installCommand, installOutput, 300000))
    {
        errorMessage = installOutput.isNotEmpty() ? installOutput : errorMessage;
        return false;
    }

    if (installOutput.containsIgnoreCase("requires restarting")
        || installOutput.containsIgnoreCase("restart"))
        needsReboot = true;

    return true;
}
} // namespace

juce::String DriverInstaller::blackHoleDownloadPageUrl()
{
    return kBlackHoleDownloadPage;
}

bool DriverInstaller::isDriverPackageAvailable()
{
    return true;
}

bool DriverInstaller::isBlackHoleHalOnDisk()
{
    return blackHoleHalExistsAt("/Library/Audio/Plug-Ins/HAL/BlackHole2ch.driver")
           || blackHoleHalExistsAt("/Library/Audio/Plug-Ins/HAL/BlackHole16ch.driver")
           || blackHoleHalExistsAt("/Library/Audio/Plug-Ins/HAL/BlackHole64ch.driver")
           || blackHoleHalExistsAt("/Library/Audio/Plug-Ins/HAL/BlackHole.driver");
}

bool DriverInstaller::restartCoreAudioServices(juce::String& message)
{
    return restartCoreAudio(message);
}

bool DriverInstaller::installVirtualMicDriver(juce::String& errorMessage)
{
    bool needsReboot = false;

    if (isBlackHoleHalOnDisk())
    {
        juce::String restartWarning;
        const bool audioRestarted = restartCoreAudio(restartWarning);

        if (! audioRestarted && restartWarning.isNotEmpty())
            errorMessage = restartWarning;

        for (int attempt = 0; attempt < 5; ++attempt)
        {
            if (input_mixer::RustBridge::detectVirtualDriver())
                return true;

            juce::Thread::sleep(400);
        }

        if (! input_mixer::RustBridge::detectVirtualDriver())
        {
            errorMessage = "The virtual microphone driver is installed, but it is not available yet.\n\n"
                           "Restart your Mac, then click Check again. After that, choose "
                           "\"Input Mixer Channel\" as your microphone.";
            return false;
        }

        return true;
    }

    juce::String downloadError;

    if (! installViaDownloadedPkg(downloadError, needsReboot))
    {
        errorMessage = downloadError;
        return false;
    }

    if (! isBlackHoleHalOnDisk())
    {
        errorMessage = "The installer finished, but the virtual microphone driver was not found in "
                       "/Library/Audio/Plug-Ins/HAL/. "
                       "Restart your Mac and click Check again.";
        return false;
    }

    juce::String restartWarning;
    const bool audioRestarted = restartCoreAudio(restartWarning);

    if (needsReboot || ! audioRestarted)
    {
        errorMessage = audioRestarted
                           ? "The virtual microphone driver was installed. macOS says a restart is required before "
                             "it appears. Restart your Mac, then click Check again."
                           : "The virtual microphone driver was installed, but audio services could not be restarted. "
                             "Restart your Mac, then click Check again.";

        if (restartWarning.isNotEmpty())
            errorMessage << "\n\n" << restartWarning;

        return false;
    }

    return true;
}

#else

juce::String DriverInstaller::blackHoleDownloadPageUrl()
{
    return {};
}

bool DriverInstaller::isDriverPackageAvailable()
{
    return false;
}

bool DriverInstaller::isBlackHoleHalOnDisk()
{
    return false;
}

bool DriverInstaller::restartCoreAudioServices(juce::String& message)
{
    message = "In-app audio restart is only available on macOS.";
    return false;
}

bool DriverInstaller::installVirtualMicDriver(juce::String& errorMessage)
{
    errorMessage = "In-app driver installation is only available on macOS.";
    return false;
}

#endif
