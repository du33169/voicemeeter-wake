<div align="center">
<img src="./resources/app.svg" style="width:4rem;" />

# Voicemeeter Wake
</div>

A small native Windows (Win32) tray application that wakes up your headphones by restarting the Voicemeeter audio engine after a long silence.

You need to have [Voicemeeter](https://www.voicemeeter.com) (any version) installed first to use this tool.

I wrote this tool for my headset; it may not work for yours. If it does, lucky you!

## Why

Some(one of my) wireless headphones/headsets go into sleep after a period of silence and cannot self-wake on new audio playback. I have to manually reboot the headset every time, which is annoying. While investigating, I found that restarting the Voicemeeter audio engine re-initializes the output device and wakes them up. Every time audio returned, I still had to click "restart audio engine" in Voicemeeter to wake my headset. To avoid that, I wrote this tool that implements the silence-to-playback trigger using the official [Voicemeeter Remote API](https://github.com/vburel2018/Voicemeeter-SDK).

## How it works

- Detects the running Voicemeeter edition and installation path from the registry, loads the official `VoicemeeterRemote64.dll` from the installation to initialize the connection.
- You select one hardware output bus (A1..A3 on Banana, A1..A5 on Potato) that routes to your headset.
- After the selected output has been silent (level < silence threshold) for the configured "arm silence" time, the monitor is armed.
- The next time playback is detected (level ≥ play threshold for 3 consecutive samples), it sends `Command.Restart = 1` to Voicemeeter, triggering the audio engine restart.
- A cooldown period and a fresh full silence window are required before it can trigger again, to avoid restart loops.

## Build

Uses [Pixi](https://pixi.sh) to provision the MinGW-w64 UCRT GCC 15 toolchain, CMake and Ninja:

```sh
pixi run configure   # CMake configure (Ninja, Release)
pixi run build       # build voicemeeter-wake.exe
pixi run test        # run the audio-monitor unit tests
pixi run run         # launch the app
```

The GCC runtime is statically linked. The executable uses Windows' system UCRT (`ucrtbase.dll`).

## Usage

- Main window shows the selected outputs' peak level, monitor state, silence timer, restart stats and a concise operation log.
- Closing the window minimizes to the tray; right-click the tray icon for Show / Pause / Restart now / Exit.
- Settings are stored under `HKCU\Software\VoicemeeterWake`; run-at-startup uses the standard `HKCU\...\Run` key.
- Operation logs are kept in memory for the current process and are discarded when the application exits.

## Notes

- Voicemeeter Standard exposes A1/A2 as one shared output level in the Remote API, so those two physical outputs cannot be monitored independently.
- Restarting the engine causes a short (~1 s) audio interruption; that is inherent to the wake-up mechanism.

## Disclaimer

Unofficial third-party tool. Not affiliated with, endorsed by, or sponsored by VB-AUDIO SOFTWARE. `Voicemeeter` is a product of VB-AUDIO SOFTWARE; it is donationware, and its author welcomes all kinds of participations.

This project dynamically loads the `VoicemeeterRemote64.dll` that is already installed with Voicemeeter (via its registry `UninstallString`). The DLL is never bundled with or redistributed by this project.
