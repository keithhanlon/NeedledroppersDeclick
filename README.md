# Needledropper's Declicker

An open source vinyl declicker for macOS, Windows, and Linux.

Inspired by Brian Davies' ClickRepair, Needledropper's Declicker uses
AR prediction-error detection and autoregressive interpolation to detect
and repair clicks and pops from digitized vinyl records with minimal
impact on the surrounding audio.

## Features

- AR prediction-error click detection — catches clicks regardless of signal amplitude
- Bidirectional autoregressive interpolation (Levinson-Durbin)
- Stereo-aware cross-channel repair
- Waveform display with click markers
- Before/after A-B audition
- Batch processing queue
- Real-time sensitivity preview

## How it works

Detection computes a forward AR prediction error at every sample and
maintains a slow exponential running average of that error. A sample is
flagged as a click when its prediction error spikes far above the local
average — a discriminator that correctly ignores loud musical passages
(cymbals, drums, vocals) while catching impulsive vinyl damage.

Repair uses bidirectional AR interpolation: forward and backward AR
models are fitted to the clean audio flanking each damaged region, and
their predictions are blended across the gap. For stereo material, the
undamaged channel is used as an additional reference constraint.

## Building

### Prerequisites

**macOS**
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`

**Windows**
- Visual Studio 2022 (Community edition is free) with the
  "Desktop development with C++" workload
- CMake (included with Visual Studio, or install separately from cmake.org)

**Linux**
- GCC 9+ or Clang 10+: `sudo apt install build-essential` (Debian/Ubuntu)
  or `sudo dnf install gcc-c++` (Fedora)
- CMake: `sudo apt install cmake` or `sudo dnf install cmake`
- JUCE dependencies:
  `sudo apt install libasound2-dev libfreetype6-dev libcurl4-openssl-dev libwebkit2gtk-4.0-dev`

### Clone
```bash
git clone https://github.com/keithhanlon/NeedledroppersDeclick.git
cd NeedledroppersDeclick
git submodule update --init --recursive
```

### Build
```bash
cmake -B build -S .
cmake --build build --config Debug
```

The app will be at `build/NeedledroppersDeclick_artefacts/Needledropper's Declicker.app`
(macOS) or the equivalent platform path.

## Usage

1. Drop one or more WAV/AIFF/FLAC files onto the window
2. Adjust the **Sensitivity** slider (default 30 — conservative)
   — lower values catch only obvious clicks
   — higher values catch more subtle damage but may affect transients
3. Enable **Reverse pass** to catch asymmetric clicks (recommended)
4. Click **Process**
5. Use the **A/B** toggle to compare before and after while playing
6. Output files are written next to the source with a `-repaired` suffix

## Status

Active development. Click detection and repair working with confirmed
clean polarity-flip tests on real vinyl material. Planned additions:
crackle detection (DeCrackle mode) and improved reverse pass integration.

## License

AGPL-3.0. See LICENSE for details.

## Acknowledgements

Inspired by the work of Brian Davies, whose ClickRepair set the standard
for transparent vinyl restoration.
