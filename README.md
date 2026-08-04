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

- Detects the running Voicemeeter edition and lets you select one
  hardware output buses (A1..A3 on Banana, A1..A5 on Potato) at 100 ms intervals.
- After the selected output has been silent for the configured "arm silence"
  time (default 600 s),
  the monitor is armed.
- The next time playback is detected (level ≥ play threshold for 3 consecutive
  samples), it calls `Command.Restart = 1` once.
- A cooldown period (default 5 s) and a fresh full silence window are required
  before it can trigger again - no restart loops.
- Launching the app while audio is already playing never triggers a restart.

## Requirements

- Windows 10/11 x64.
- Voicemeeter Standard, Banana, or Potato installed (official Remote API DLL `VoicemeeterRemote64.dll`
  is loaded from the install directory - it is never bundled).
- The target headset must be assigned to the selected hardware output.

## Build

Uses [Pixi](https://pixi.sh) to provision the MinGW-w64 UCRT GCC 15 toolchain,
CMake and Ninja:

```sh
pixi run configure   # CMake configure (Ninja, Release)
pixi run build       # build voicemeeter-engine-wake.exe
pixi run test        # run the audio-monitor unit tests
pixi run run         # launch the app
```

The GCC runtime is statically linked. The executable uses Windows' system UCRT
(`ucrtbase.dll`) and does not require a private compiler-runtime DLL.

## Usage

- Main window shows the selected outputs' peak level, monitor state, silence timer, restart stats
  and a concise operation log.
- Closing the window minimizes to the tray; right-click the tray icon for
  Show / Pause / Restart now / Exit.
- Settings are stored under `HKCU\Software\VoicemeeterEngineWake`;
  run-at-startup uses the standard `HKCU\...\Run` key.
- Operation logs are kept in memory for the current process and are discarded
  when the application exits.

## Notes

- Voicemeeter Standard exposes A1/A2 as one shared output level in the Remote
  API, so those two physical outputs cannot be monitored independently.
- Restarting the engine causes a short (~1 s) audio interruption; that is
  inherent to the wake-up mechanism.
