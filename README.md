# AI Synth (VSTi)

A JUCE-based synthesizer project designed to run in Cubase and other plugin hosts.

By default this project builds **VST3 + Standalone**. Optional legacy **VST2 (.dll)** output can be enabled for personal/local builds (for older hosts like Cubase 5) when you provide a local VST2 SDK path.

## Features
- Oscillators: sine, saw, square, noise
- FM oscillator (ratio, amount, mix)
- ADSR with curve control and stepped mode
- Envelope visualization in plugin UI
- Modulation routing controls (LFO->filter, ENV->filter)
- Unison up to 128 voices with stereo pan + Hz spread
- Filter + built-in FX: delay, phaser, flanger, distortion/overdrive
- GitHub Actions build for VST3 artifacts (macOS/Windows/Linux)
- Optional local VST2 build mode for legacy hosts (e.g. Cubase 5)

## Build locally
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target AISynth_VST3
```

The generated `.vst3` bundle will appear in the `build` tree in the platform-specific plugin output folder.

## Optional: Build a VST2 `.dll` for Cubase 5
> This requires a locally installed VST2 SDK path. This repository does **not** include or distribute the SDK.

On Windows PowerShell:
```powershell
$env:VST2_SDK_DIR = "C:\path\to\vst2_sdk"
cmake -S . -B build-vst2 -DAISYNTH_ENABLE_VST2=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-vst2 --config Release --target AISynth_VST
```

If `VST2_SDK_DIR` is missing, CMake will warn and continue building VST3/Standalone only.
