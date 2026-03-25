# Needledropper's Declicker

An open source vinyl declicker for macOS, Windows, and Linux.

Inspired by Brian Davies' ClickRepair, Needledropper's Declicker uses wavelet
decomposition and autoregressive interpolation to detect and repair clicks and
pops from digitized vinyl records with minimal impact on the surrounding audio.

## Features

- Wavelet-based click detection (Daubechies D4)
- Bidirectional autoregressive interpolation
- Stereo-aware cross-channel repair
- Forward and reverse processing passes
- Waveform display with click markers
- Before/after A-B audition
- Batch processing queue
- Real-time sensitivity preview

## Building

### Prerequisites

**macOS**
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`

**Windows**
- Visual Studio 2022 (Community edition is free) with the "Desktop development with C++" workload
- CMake (included with Visual Studio, or install separately from cmake.org)

**Linux**
- GCC 9+ or Clang 10+: `sudo apt install build-essential` (Debian/Ubuntu) or `sudo dnf install gcc-c++` (Fedora)
- CMake: `sudo apt install cmake` or `sudo dnf install cmake`
- JUCE dependencies: `sudo apt install libasound2-dev libfreetype6-dev libcurl4-openssl-dev libwebkit2gtk-4.0-dev`

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

## Status

Currently in early development. DSP core and UI scaffold in place.

## License

AGPL-3.0. See LICENSE for details.

## Acknowledgements

Inspired by the work of Brian Davies, whose ClickRepair set the standard
for transparent vinyl restoration.
