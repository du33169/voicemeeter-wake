# Voicemeeter Engine Wake

A small native Windows (Win32) tray application that wakes up your headphones by
restarting the Voicemeeter audio engine once, when audio resumes after a long
silence.

## Why

Some headphones/headsets (especially USB) go into sleep and ignore the first
audio after silence. Restarting the Voicemeeter audio engine re-initializes the
output device and wakes them up.

The built-in menu option `Auto Restart Audio Engine (A1/All Device)` is a
*device reconnection* recovery feature - it does **not** trigger on
silence-to-playback transitions, so it does not solve this problem. This tool
implements the silence-to-playback trigger using the official
[Voicemeeter Remote API](https://github.com/vburel2018/Voicemeeter-SDK).

## How it works

- Monitors the Voicemeeter Banana **A1** output bus peak level (`VBVMR_GetLevel(3, 0..7, ...)`)
  at 100 ms intervals.
- After A1 has been silent for the configured "arm silence" time (default 600 s),
  the monitor is armed.
- The next time playback is detected (level ≥ play threshold for 3 consecutive
  samples), it calls `Command.Restart = 1` once.
- A cooldown period (default 5 s) and a fresh full silence window are required
  before it can trigger again - no restart loops.
- Launching the app while audio is already playing never triggers a restart.

## Requirements

- Windows 10/11 x64.
- Voicemeeter Banana installed (official Remote API DLL `VoicemeeterRemote64.dll`
  is loaded from the install directory - it is never bundled).
- The target headset must be assigned to **A1**.

## Build

Uses [Pixi](https://pixi.sh) to provision the MinGW-w64 UCRT GCC 15 toolchain,
CMake and Ninja:

```sh
pixi run configure   # CMake configure (Ninja, Release)
pixi run build       # build voicemeeter-engine-wake.exe
pixi run test        # run the audio-monitor unit tests
pixi run run         # launch the app
```

The final executable is statically linked (no GCC runtime DLLs) and depends only
on Windows system libraries plus the UCRT.

## Usage

- Main window shows live A1 level, monitor state, silence timer, restart stats
  and a concise operation log.
- Closing the window minimizes to the tray; right-click the tray icon for
  Show / Pause / Restart now / Exit.
- Settings are stored under `HKCU\Software\VoicemeeterEngineWake`;
  run-at-startup uses the standard `HKCU\...\Run` key.
- Log file: `%LOCALAPPDATA%\VoicemeeterEngineWake\app.log` (rotated at 512 KB).

## Notes

- Only Voicemeeter **Banana** is supported (Potato/Standard show a warning and
  are ignored).
- Restarting the engine causes a short (~1 s) audio interruption; that is
  inherent to the wake-up mechanism.
