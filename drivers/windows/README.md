# Input Mixer virtual microphone driver (Windows)

This directory contains the Windows virtual audio driver based on the Microsoft
[SysVAD / Simple Audio Sample](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/simpleaudiosample).

## Status

Production requires a signed kernel-mode driver (EV code signing certificate) that
exposes a virtual **capture** endpoint named **Input Mixer**.

## Build (production)

1. Install Visual Studio + Windows Driver Kit (WDK)
2. Fork Simple Audio Sample / SysVAD
3. Implement user-mode IOCTL or shared-buffer path for mixed PCM from `input-mixer-core`
4. Package as signed `.msi` in `installer/windows/`

## Dev fallback

Without the driver, mixed audio is available via the monitor output only.
Process loopback capture uses WASAPI `AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK`
(Windows 10 version 2004+).

## User-mode feeding

The Rust engine writes 48 kHz stereo float PCM to the virtual device via `cpal`
once the driver endpoint is installed.
