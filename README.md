# AI Synth (VSTi)

A JUCE-based VST3 synthesizer project designed to run in Cubase and other VST3 hosts.

## Features
- Oscillators: sine, saw, square, noise
- FM oscillator (ratio, amount, mix)
- ADSR with curve control and stepped mode
- Envelope visualization in plugin UI
- Modulation routing controls (LFO->filter, ENV->filter)
- Unison up to 128 voices with stereo pan + Hz spread
- Filter + built-in FX: delay, phaser, flanger, distortion/overdrive
- GitHub Actions build for VST3 artifacts (macOS/Windows/Linux)

## Build locally
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target AISynth_VST3
```

The generated `.vst3` bundle will appear in the `build` tree in the platform-specific plugin output folder.
