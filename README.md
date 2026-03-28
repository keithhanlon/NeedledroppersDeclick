# Needledropper's Declicker

An open source vinyl declicker for macOS, Windows, and Linux.

Needledropper's Declicker uses
AR prediction-error detection and autoregressive interpolation to detect
and repair clicks and pops from digitized vinyl records with minimal
impact on the surrounding audio.

## Features

- AR prediction-error click detection; catches clicks regardless of signal amplitude
- Bidirectional autoregressive interpolation (Levinson-Durbin)
- Stereo-aware cross-channel repair
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
2. Adjust the **Sensitivity** slider (default 30; conservative)
   - lower values catch only obvious clicks
   - higher values catch more subtle damage but may affect transients
3. Enable **Reverse pass** to catch asymmetric clicks (recommended)
4. Click **Process**
5. Use the **A/B** toggle to compare before and after while playing
6. Output files are written next to the source with a `-repaired` suffix

## Status

Active development. Click detection and repair working with confirmed
clean polarity-flip tests on real vinyl material.

## License

AGPL-3.0. See LICENSE for details.

## Algorithms
Needledropper's Declicker is built on the following published algorithms:

- **Levinson-Durbin AR fitting** - efficient autoregressive model estimation via the Levinson-Durbin recursion, used for both click detection and repair
- **AR prediction-error click detection** - the foundational approach established by Vaseghi & Rayner (1990), "Detection and suppression of impulsive noise in speech communication systems"; a practical walkthrough is available in [Essentia's ClickDetector tutorial](https://essentia.upf.edu/tutorial_audioproblems_clickdetector.html)
- **MAD sigma estimation** - Median Absolute Deviation with the 1.4826 Gaussian consistency factor (Donoho & Johnstone, 1994), used to estimate noise floor from wavelet detail coefficients
- **DWT/IDWT with Daubechies D4 coefficients** - four-coefficient orthogonal wavelet transform for multi-level signal decomposition
- **NSDF pitch detection** - Normalized Square Difference Function pitch estimator (McLeod & Wyvill, 2005), used to select pitch-synchronous repair context on tonal material

## Originality

Needledropper's Declicker is an original work. All source code was written independently and does not derive from, copy, or incorporate code from any existing software, including any existing software.

The algorithms employed (Levinson-Durbin AR fitting, MAD sigma estimation, Daubechies D4 wavelet transform, and NSDF pitch detection) are well-established methods published in the open academic literature and are not proprietary to any software product. Their application here represents an independent implementation based on published descriptions.

Copyright © 2025 Keith Hanlon. Released under the AGPL-3.0 license.

## Acknowledgements

Inspired by the work of Brian Davies, whose ClickRepair set the standard
for transparent vinyl restoration.

Davies explains the underlying approach in his own words in this 2013
interview: https://youtu.be/3dAFhhbwGtQ?si=-38o-5zMus4Ea5D2
