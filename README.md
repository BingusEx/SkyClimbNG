# SkyClimb
SkyClimb brings a true climbing mechanic to Skyrim. Utilizing EVG Traversal's animations, SkyClimb uses techniques seen in modern games to allow you to realistically climb onto anything.

## Changes
 - Updated Commonlib.
 - Support for 1.5.97 / 1.6.1170.
 - Character height is now taken into account for calculating the EVG marker position.

## Requirements
 - CMake
 - Vcpkg
 - Visual Studio 2022 with Desktop C++ Development
 - [CommonLib](https://github.com/alandtse/CommonLibVR/tree/ng)
	- Add the environment variable `CommonLibSSEPath_NG` with the value as the path to the folder containing CommonLib

## Building
```
git clone https://github.com/BingusEx/SkyClimbNG
cd SkyClimbNG

cmake --preset vs2022-windows
cmake --build build --config Release
```