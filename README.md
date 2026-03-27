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

Each sample's AR forward prediction error is compared to a slow-decaying
running average of recent prediction error. Clicks spike sharply above
that average; musical transients don't. Damaged regions are repaired by
blending forward and backward AR predictions fitted to the surrounding
clean audio.

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
clean polarity-flip tests on real vinyl material.

**Roadmap (in order):**

- **Crackle detection** — separate DeCrackle mode using energy-sum detection for broad, sustained surface noise
- **Reverse pass** — run detection on the time-reversed signal and merge results to catch asymmetric clicks
- **Output modes** ✓ — A/B/Delta (noise-only) transport modes after offline processing; real-time paced processing with live A/B/Delta monitoring is a future phase
- **Stereo to mono downmix** — optional pre-processing to improve detection on poorly balanced pressings
- **Pitch protection** — detect pitched regions and apply stricter repair constraints to avoid artifacts on sustained tones
- **UI polish** — sensitivity labels, real-time click count while dragging, waveform zoom; click markers as thin tick marks rather than filled overlays (currently renders as solid red on dense material); absolute amplitude gate for quiet pressings

## License

AGPL-3.0. See LICENSE for details.

## Acknowledgements

Inspired by the work of Brian Davies, whose ClickRepair set the standard
for transparent vinyl restoration.

Davies explains the underlying approach in his own words in this 2013
interview: https://youtu.be/3dAFhhbwGtQ?si=-38o-5zMus4Ea5D2
