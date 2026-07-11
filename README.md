SonyHeadphonesClient 
===

A spiritual successor to  [Plutoberth's original SonyHeadphonesClient](https://github.com/Plutoberth/SonyHeadphonesClient) - now with standardized support for newer devices and more platforms. (Forked from https://github.com/mos9527/SonyHeadphonesClient)


<img width="1286" height="1120" alt="Screenshot_20260711_200333" src="https://github.com/user-attachments/assets/888830e6-2f7a-41a9-a44d-f97cc0d21154" />


## Roadmap
This brach is expected to be merged/released once the following features have been implemented.
- [ ] Support for legacy (`v1` protocol) devices, e.g. WH-1000XM4, WH-1000XM3
- [x] Native macOS platform support

## Compatiblity

The following platforms (applies to `libmdr`, `client`) are *natively* supported with first-party effort.

| Platform         | Support Status | Maintainers          |
|------------------|----------------|----------------------|
| Windows          | Full Support   | @mos9527, @Amrsatrio |
| Linux            | Full Support   | @mos9527             |
| macOS            | Full Support   | @mos9527             |
| Web (Emscripten) | Full Support   | @mos9527             |

For device support, refer to `docs/device-support` to check. If the feature support status for your own device is missing/incorrect/untested here, feel free to submit an [Issue](https://github.com/mos9527/SonyHeadphonesClient/issues/new) so we can work on it!

## Notes on Web Platform

The client app is available as a Progressive Web App with exact UI/Feature parity seen in other platforms.

**Live version is available** at: https://mos9527.com/SonyHeadphonesClient/

A [Web Serial](https://caniuse.com/wf-serial) supporting browser is required - with a minor exception of Chrome on Android where [Web serial over Bluetooth on Android](https://cr-status.appspot.com/feature/5139978918821888) is supported as of Android build 138. You can expect the app to work on all reasonably new Desktop Chrome browsers, and the latest Android Chrome builds.

As always, status reports are welcome - please do submit an Issue if your browser supports the Web version of the client app.

## For Developers

We have extensive documentations available in the source files. Moreover, refer to the respective README files in each source folder to understand what they do!

A C++20 compliant compiler is required. GCC 14, Clang 21 and MSVC 19 has been used for development and are guanrateed to be supported.

### Building (Regular CMake)
This is no different from your regular CMake projects.
Third-party dependencies (see `contrib`) are managed by CMake's `FetchContent` and are always statically linked, so no worries - Expect things to *just work*.

**For Developers:** See also `tooling` for codegen dependencies.
#### Building on Linux
You need DBus and BlueZ development packages installed.
- Debian (Ubuntu): `sudo apt install libbluetooth-dev libdbus-1-dev`
- Fedora: `sudo dnf install bluez-libs-devel dbus-devel`
- Arch Linux: `sudo pacman -S bluez dbus`

**For Developers:** You may want to have `bluez-tools`/`bluez-utils` installed for testing.
#### Example
```bash
mkdir build
cd build
cmake ..
cmake --build . --target SonyHeadphonesClient
```

### Building (emscripten)
- Install the SDK with `emsdk` and verify your installation - https://emscripten.org/docs/getting_started/downloads.html
- Run `emcmake cmake ...` in place of configuration.
  - Or, set up CMake Toolchain variables manually with e.g. 
  ```-DCMAKE_TOOLCHAIN_FILE=/usr/lib/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_CROSSCOMPILING_EMULATOR=/usr/lib/emsdk/node/20.18.0_64bit/bin/node```
- No extra dependencies are required.
- Example usage
```bash
mkdir build
cd build
emcmake cmake ..
cmake --build . --target SonyHeadphonesClient
```
- Run the generated page with `emrun client/index.html` - or host it with any static HTTP server.

### IntelliJ CLion/Rider (emscripten)
Set up CMake Toolchain variables with a new CMake Profile (in CMake options) - you can 
then build from there and serve the static content within `<build directory>/client/` with any static HTTP server.

## Platform Quirks
### Linux
On Linux, you may not see player metadata (track title, artist, etc.) despite correct output from `playerctl metadata` command
while your device having proper AVRCP support (e.g. works on other platforms).
- You (or your distro) may have misconfigured [MPRIS](https://wiki.archlinux.org/title/MPRIS) service. One way to remedy
  is to manually run `mpris-proxy` (available in `bluez-tools`/`bluez-utils` package) in the background, or as a systemd service.
- See also
  - https://wiki.archlinux.org/title/MPRIS
  - https://github.com/bluez/bluez/issues/868

## New Features (this fork)

- **ULT Power Sound / Sound Effect Mode** — Added full ULT mode control (OFF/ULT1/ULT2) and sound effect selection (OFF/ULT/ULT1/ULT2/CUSTOM) for ULT series devices like WH-ULT900N "ULT WEAR", extracted from the official Sony Headphones Connect app via APK decompilation.
- **Device Photo Display** — Automatically fetches and displays product images from Sony's GraphQL API (`v1.api.data-gateway.seeds.services/graphql`), using an API key extracted from `libcloudmodelinfo.so` via Ghidra/static analysis. Falls back to bundled default images from the official app.
- **Device Image Database** — Local cache of ~300+ Sony device model entries with CDN-hosted product photos, sourced from decompiling the official Sony Headphones Connect APK (v13.0.5).
- **Missing Protocol Features** — Implemented handler code for `SOUND_EFFECT`, `PRESET_EQ_AND_ULT_MODE` (ULT mode), and added missing enum values (`ULT_SOUND_EFFECT_ASSIGN`, `CUSTOMIZABLE_SOUND_EFFECT`, `SOUND_EFFECT_FLAT`, `SOUND_EFFECT_LIVE`, etc.) derived from decompiling the official app. See `Report.md` for a full gap analysis.

## Acknowledgements

This project is a fork of the excellent work by [mos9527](https://github.com/mos9527/SonyHeadphonesClient), which itself is a rewrite of the original [SonyHeadphonesClient by Plutoberth](https://github.com/Plutoberth/SonyHeadphonesClient). Many protocol definitions, feature discoveries, and the device image database are derived from decompiling the official Sony Headphones Connect APK (v13.0.5) using jadx, with additional native library analysis via Ghidra. 
