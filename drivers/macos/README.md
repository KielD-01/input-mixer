# Input Mixer virtual microphone driver (macOS HAL)

This directory contains the macOS HAL audio driver that exposes **Input Mixer** as a
system audio device.

## Status

The driver follows the [BlackHole](https://github.com/ExistentialAudio/BlackHole) HAL
plug-in pattern. A full fork with custom bundle ID `audio.inputmixer.driver` is
required for production.

## Build (production)

1. Fork BlackHole or implement a 2-channel loopback HAL driver
2. Brand the device as **Input Mixer**
3. Sign and notarize the `.pkg` installer in `installer/macos/`
4. First-run wizard detects the device via CoreAudio enumeration

## Dev fallback

Without the production driver installed, Input Mixer installs BlackHole as a
loopback backend and creates a CoreAudio aggregate device named **Input Mixer Channel**
so Chrome, Meet, and other apps show that name in the microphone list. Mixed audio
is still routed through the loopback driver; users should select **Input Mixer Channel**,
not BlackHole.

Without any loopback driver, mixed audio goes to the monitor output (default
speakers/headphones). The app status shows **Virtual microphone not installed**.

## Install location

```
/Library/Audio/Plug-Ins/HAL/InputMixer.driver
```

After install, restart CoreAudio:

```bash
sudo launchctl kickstart -kp system/com.apple.audio.coreaudiod
```
