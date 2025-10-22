# L2 CV Bot - Modern Windows 10/11 Edition

Simple Lineage II bot that uses Computer Vision (OpenCV) to find possible targets and monitor HP/MP/CP. **Updated for modern Windows 10/11 with latest libraries and no driver dependencies!**

[![Video](preview.png)](https://youtu.be/nNgeUZIxllY "Demonstration video")

## ✨ What's New in This Version

- 🚀 **No Driver Installation Required** - Uses native Windows APIs instead of Interceptor driver
- 🔧 **Modern Build System** - Updated for Visual Studio 2022 and latest CMake
- 📚 **Latest Libraries** - Compatible with OpenCV 4.x and modern C++ standards
- 🖥️ **Enhanced Debug Overlay** - New GDI+ based overlay system for better debugging
- 📖 **Comprehensive Setup Guide** - Step-by-step instructions for modern Windows

## Features

* Near and far NPC detection
* HP/MP/CP monitoring
* Mouse and keyboard emulation (no driver required!)
* Modern debug overlay system
* ~~Stuck resolving~~
* ~~TTS alarm subsystem (captcha, low HP, CP decreasing, etc.)~~
* ~~Custom behavior support (LUA scripts)~~
* ~~Buffs/debuffs monitoring~~

## Quick Start

**No driver installation needed!** Just build and run.

1. **Build the project**: Run `build-modern.bat` (see [Building](#building) section)
2. **Start Lineage II** and log into your character
3. **Teleport to a farming location** (bot works best in open areas)
4. **Run the bot**: `run.bat "<title of the Lineage II client window>"`
5. **Reset bars if needed**: Press Space if HP/MP/CP bars aren't at 100% when starting
6. **Stop the bot**: Press ESC or move mouse

Default keyboard layout:

* F1 - Primary attack
* F2 - Next target
* F3 - Spoil
* F4 - Sweep
* F5 - Pick up
* F6 - Restore HP when <70%
* F7 - Restore MP when <70%
* F8 - Restore CP when <90%

## Command line options

`l2-cv-bot.exe`:

```
--window    Lineage II window title or part of the title. Default: "Lineage II"
--debug     Show window with debug information. Default: true
```

`run.bat` only accepts window title.

## Customization

Current version developed and tested using Windows 10 and Gracia Epilogue client, so with another Windows or Lineage II client it may not work.

* Edit `run.bat` to customize CV or keyboard layout for another client. Note that for colors are used HSV and **B**G**R** color models.
* Edit `Brain.cpp` to customize bot behavior and timings. Custom runtime behavior scripts currently aren't supported.
* OS related stuff placed in these files: `Window.cpp`, `Capture.cpp`, `Input.cpp`, `Intercept.cpp`.

## Building

### Prerequisites
- **Visual Studio 2022** (Community edition is fine)
- **CMake 3.20+** 
- **OpenCV 4.x** for Windows

### Quick Build (Recommended)
```cmd
# Set OpenCV path
set OpenCV_DIR=C:\opencv\build

# Build with modern script
build-modern.bat
```

### Manual Build
```cmd
# Configure
cmake -H. -Bbuild -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release --target INSTALL
```

### Build Output
Built executable and dependencies will be in `build-modern\Release\` directory.

## Detailed Setup

For comprehensive setup instructions, see [SETUP_GUIDE.md](SETUP_GUIDE.md).

## Technical Details

- **Input Handling**: Uses native Windows APIs (`SetWindowsHookEx`, `SendInput`) - no drivers needed
- **Graphics**: GDI+ based debug overlay system
- **Compatibility**: Windows 10/11, Visual Studio 2022, OpenCV 4.x
- **Architecture**: 64-bit builds recommended