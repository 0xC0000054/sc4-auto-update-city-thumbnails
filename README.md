# sc4-auto-update-city-thumbnails

A DLL Plugin for SimCity 4 that adds a cheat code to automate updating the city thumbnails in the region view.   

The plugin can be downloaded from the Releases tab: https://github.com/0xC0000054/sc4-auto-update-city-thumbnails/releases

## System Requirements

* SimCity 4 version 641
* Windows 10 or later
* [Microsoft Visual C++ 2022 x86 Redistribute](https://aka.ms/vs/17/release/vc_redist.x86.exe)

The plugin may work on Windows 7, but I do not have the ability to test that.

## Installation

1. Close SimCity 4.
2. Copy `SC4AutoUpdateCityThumbnails.dll` and `SC4AutoUpdateCityThumbnails.ini` into the top-level of the Plugins folder in the SimCity 4 installation directory or Documents/SimCity 4 directory.
3. Start SimCity 4.

## Usage

> ### ⚠️ Warning
> **You should make a backup of the region/cities before running this cheat.**
>
> There is a chance the cheat will crash the game or cause other issues.

The DLL adds an _AutoUpdateCityThumbnails_ cheat to the region view.
This cheat will load each city in the region and save it to update the region view thumbnail before moving on to the next city.
Due to the way SC4 works, the game must remain in focus while it loads and saves cities.

The _AutoUpdateCityThumbnails_ cheat has the following syntax:

AutoUpdateCityThumbnails [city type]

The _[city type]_ parameter is optional, and can be one of the values from the following table:

| Name | Description |
|------|-------------|
| all | Both established (Mayor mode) and unestablished (God mode) cities are updated. |
| established | Only established (Mayor mode) cities are updated. |
| unestablished | Only unestablished (God mode) cities are updated. |

If the _[city type]_ parameter is omitted, the command will default to processing only unestablished (God mode) cities.

## Troubleshooting

The plugin should write a `SC4AutoUpdateCityThumbnails.log` file in the same folder as the plugin.    
The log contains status information for the most recent run of the plugin.

# License

This project is licensed under the terms of the GNU Lesser General Public License version 2.1 or (at your option) any later version published by the Free Software Foundation.    
See [LICENSE.txt](LICENSE.txt) for more information.

## 3rd party code

[gzcom-dll](https://github.com/nsgomez/gzcom-dll/tree/master) - LGPL 2.1 or later License.    
[sc4-dll-utilities](https://github.com/0xC0000054/sc4-dll-utilities) - LGPL 2.1 or later License.   
[SC4Fix](https://github.com/nsgomez/sc4fix) - MIT License.     
[Windows Implementation Library](https://github.com/microsoft/wil) - MIT License.    

# Source Code

## Prerequisites

* Visual Studio 2022
* `git submodule update --init`

## Building the plugin

* Open the solution in the `src` folder
* Update the post build events to copy the build output to you SimCity 4 application plugins folder.
* Build the solution

## Debugging the plugin

Visual Studio can be configured to launch SimCity 4 on the Debugging page of the project properties.
I configured the debugger to launch the game in full screen with the following command line:    
`-intro:off -CPUcount:1 -w -CustomResolution:enabled -r1920x1080x32`

You may need to adjust the resolution for your primary screen.
