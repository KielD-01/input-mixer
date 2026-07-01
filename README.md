# Input Mixer

Mix up to six unique audio sources (microphones, apps, games) into one virtual microphone for use in Discord, Skype, Telegram, Slack, and other apps.

## Architecture

- **Rust core** (`crates/input-mixer-core`) — capture, mixing, metering, platform audio APIs
- **JUCE shell** (`shell/`) — UI, first-run wizard, settings persistence
- **cxx bridge** — type-safe FFI between C++ and Rust (no PCM crosses the boundary)

## Requirements

- macOS 13+ or Windows 10 2004+
- Rust toolchain (`rustup`)
- CMake 3.22+
- Xcode Command Line Tools (macOS) or Visual Studio 2022 (Windows)

## Build

```bash
# Build Rust core (generates cxx bridge headers)
cargo build --release

# Build JUCE app
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The app binary is `build/shell/InputMixer_artefacts/Release/Input Mixer.app` on macOS.

## First run

1. Welcome and overview
2. Virtual microphone — detect, or install in-app on macOS
3. Hardware input/output scan
4. Permission guidance (microphone + screen recording on macOS)
5. Main mixer window

On macOS, the setup wizard can install the bundled virtual microphone package
without leaving the app. Click **Install virtual microphone** on the driver step;
macOS will prompt for your administrator password. The app then restarts CoreAudio
and checks again automatically.

The bundled development package (`installer/macos/create-dev-pkg.sh`) installs
support files and restarts audio services. The production HAL driver
(`InputMixer.driver`) will replace the interim loopback driver when ready.

On macOS, setup installs a loopback audio driver and registers **Input Mixer Channel**
as the microphone name other apps should use (via a CoreAudio aggregate device).
You do not need to select BlackHole manually in meeting apps.

## Features

- Up to 6 channels with unique sources (hardware or application)
- Per-channel volume, mute, input/output meters
- Master volume and limiter
- **Listen to mix** — monitor on headphones/speakers (off by default)
- Settings persist across launches (JSON via JUCE PropertiesFile)

## Linux (Phase E)

PipeWire-based virtual mic and per-app capture are implemented as stubs in
`crates/input-mixer-core/src/capture/linux/`. See plan Phase E for full support.

## Drivers

Custom virtual microphone drivers are required for other apps to see **Input Mixer**
as a mic. See `drivers/macos/` and `drivers/windows/` for production build notes.

## License

MIT
