# L2 CV Bot - Modern Windows 10/11 Setup Guide

This guide will help you build and run the Lineage II Computer Vision Bot on modern Windows 10/11 systems. **No driver installation required!**

## What This Project Does

This is a **Lineage II bot** that uses computer vision (OpenCV) to:
- Automatically detect NPCs and targets in the game
- Monitor HP/MP/CP bars
- Automate combat actions (attack, spoil, sweep, etc.)
- Use mouse and keyboard emulation for game control (via native Windows APIs)

## Prerequisites

### 1. Install Visual Studio 2022
- Download and install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)
- Make sure to include "Desktop development with C++" workload
- Include Windows 10/11 SDK

### 2. Install CMake
- Download and install [CMake 3.20+](https://cmake.org/download/)
- Make sure CMake is added to your system PATH

### 3. Install OpenCV
- Download [OpenCV 4.x](https://opencv.org/releases/) for Windows
- Extract to a folder like `C:\opencv`
- Set the `OpenCV_DIR` environment variable:
  ```cmd
  set OpenCV_DIR=C:\opencv\build
  ```

### 4. ✅ No Driver Installation Required!
Unlike the original version, this modernized bot uses native Windows APIs for input handling, so **no Interceptor driver installation is needed**!

## Building the Project

### Option 1: Use the Modern Build Script (Recommended)
```cmd
build-modern.bat
```

### Option 2: Manual Build
```cmd
# Configure
cmake -H. -Bbuild -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Release --target INSTALL
```

## Running the Bot

1. **Start Lineage II** and log into your character
2. **Teleport to a farming location** (bot works best in open areas)
3. **Make sure HP/MP/CP bars are at 100%** when starting
4. **Run the bot:**
   ```cmd
   cd build-modern\Release
   run.bat "Lineage II"
   ```
   (Replace "Lineage II" with your actual game window title)

**Note**: No driver installation or admin privileges required!

## Controls

- **Space** - Reset HP/MP/CP bar positions (if not at 100% when starting)
- **ESC** - Stop the bot
- **Mouse movement** - Stop the bot
- **Print Screen** - Save screenshot to `shot.png`

### Default Key Bindings
- **F1** - Primary attack
- **F2** - Next target  
- **F3** - Spoil
- **F4** - Sweep
- **F5** - Pick up items
- **F6** - Restore HP (when <70%)
- **F7** - Restore MP (when <70%)
- **F8** - Restore CP (when <90%)

## Troubleshooting

### Build Issues
- **"OpenCV_DIR not set"**: Set the environment variable to your OpenCV build folder
- **"CMake not found"**: Install CMake and add it to PATH
- **"Visual Studio not found"**: Install Visual Studio 2022 with C++ workload

### Runtime Issues
- **"Can't find window"**: Make sure Lineage II is running and use the correct window title
- **Bot not detecting targets**: Adjust color thresholds in `run.bat` for your specific game client
- **Input not working**: Make sure the bot has focus and Lineage II window is active

### Game Compatibility
- Originally developed for **Gracia Epilogue** client
- May need color threshold adjustments for other Lineage II versions
- Tested on Windows 10, should work on Windows 11

## Customization

### Adjust Detection Parameters
Edit `run.bat` to modify:
- Color ranges for NPC names, HP bars, etc.
- Detection thresholds
- Key bindings

### Modify Bot Behavior
Edit `src/Brain.cpp` to change:
- Combat logic
- Timing delays
- Action sequences

## Important Notes

⚠️ **This bot is for educational purposes only**
- Use at your own risk
- May violate game terms of service
- Test in safe areas first

🔧 **Technical Requirements**
- Windows 10/11
- Visual Studio 2022
- CMake 3.20+
- OpenCV 4.x
- **No driver installation required!**

📝 **Project Status**
- Originally created: 2017 (7 years ago)
- **Modernized**: 2025 for Windows 10/11 compatibility
- **Major Update**: Removed Interceptor dependency, added native Windows API support
- OpenCV APIs updated for latest versions
- Enhanced with modern debug overlay system
