# ProjectB


Play Atomic Bomberman with up to 10 keyboards on X86 and Raspi 4+.

This project provides information about running multiplayer games, specifically Atomic Bomberman, on Linux based systems with local massive multiplayer option by connecting 10 USB Keyboards. Beside x86 based systems, thanks to Wine-Hangover it is possible to use RasPi 4 (and probably but untested 5)
This repo provides tools, patches and ressources to do so. Feel free to use & modify it for other games. These instructions are tested on Ubuntu 24.04 (Ubuntu on Raspi 25.04), if you use another linux OS, you are probably capable of porting these instructions.

# Keyboard to Joystick wrapper

## General Idea

This repo include the key2joy tool, a small C tool, that creates a virtual joystick for each physical keyboard. The keyboard devices are directly tapped by opening /dev/input/event*. In order to do so, we need root rights, because it does the same as a malicious keylogger and reads all key presses in background. (and needs to do so, but better have a look at the C code). 

```
cd key2joy
make
sudo ./key2joy 
```

should do the job. Keep the terminal alive.

## Special functions

### Muting

In-game menus will receive double events, once from keyboard, once from the virtual joystick. 
Mute it by Press&Release left Ctrl. -->KB lights will come up 
Unmute it by Press&Release left Ctrl again. -->KB lights return to initial state 

### Assigning Keyboard Layout

Mute it by Press&Release left Ctrl. -->KB lights will come up 
Hit (Press&Release) K
Hit Keys for Up, Down, Left, Right, Button 1 (place bombs), Button 2 (trigger bomb, boxing glove) in that order
Unmute it by Press&Release left Ctrl again. -->KB lights return to initial state 

New Layout is stored for that Keyboard. 
Note: 

```
-Default layout: Arrow Keys, Button 1: 1, Button 2: 2
-This has to be done after each program start
-dont assign keys that will exit the game (Key Z, F-Keys)
```

### Assigning Joystick Names

(Needs Patched Wine, see below)
A real Bomberman player will get pissed if he has to play the wrong color....
```
Mute it by Press&Release left Ctrl. -->KB lights will come up 
Hit (Press&Release) N
Type your Name
Unmute it by Press&Release left Ctrl again. -->KB lights return to initial state 
```
New Name is assigned for the virtual joystick of that Keyboard. 
Note: 
```
-Default: Device name of Keyboard
-Change names before you start Bomberman! (Changing afterwards will even decouple that joystick instance, as under the hood it is unregistered and registered again)
-Newer Versions of Wine do not forward the Device Name to the Windows application, see patch below
```
# Launching Wine, Bugs & Workarounds

Before starting Atomic Bomberman in Wine, you need to configure the CFG.INI file, to make the path given there match the Windows-Style mount path in Wine, which looks (foldername AtomicBomberman) kind of:
```
Z:\home\my_linux_username\Spiele\ATOM~UPD
```
Next, make sure to cd to the Bomberman Directory (otherwise it will not find the CFG.INI) before you start with...
```
wine ./BM.exe
```


Unfortunately perfect gameplay does not work out of the box. Here are the specific obstacles in our way.

## Wayland Non Fullscreen Rendering & Physical Resolution Limitation

Running Wayland as a display server, applications are not allowed to change display resolution (for good reason). Atomic Bomberman gets rendered in a 640x480 area in the top left corner of your screen. The correct approach would be to change physical resolution before program launch, but for some reasons I dont know, minimum resolution in Wayland is 800x600 and erroneously reports that 640x480 is not supported by the screen (at least on all systems I have tested). There are two options to achieve fullscreen.

### Use Custom ddraw.dll for upscaling (only on X86)

Download the latest release from https://github.com/FunkyFr3sh/cnc-ddraw and unzip it to the game folder. Edit the file ddraw.ini and set 
```
fullscreen=true. 
```
Start wine with:
```
WINEDLLOVERRIDES="ddraw.dll=n" wine ./BM.exe (telling Wine not to use the builtin ddraw)
```
In the ddraw.ini file there is also a Atomic Bomberman specific section stating:
```
; Atomic Bomberman
[BM]
maxgameticks=60
```

Without that setting (and without custom ddraw.dll), the Bomb wobbles very fast in the game menu. I prefer to delete that lines, as potentially you might get less input lag. At least I know for sure, that without that setting, the joysticks are queried at a much higher rate. I did not test high refresh rate monitors, but it might even be possible to have more fluid rendering without that setting.

If you want to keep it, but your BM.exe is called differently (e.g. Bm98.exe), adapt it here.

Although performance impact may be neglectible on modern systems, upscaling VGA to FullHD may increase system load and lag compared to hardware-upscaling inside your screen. (or the other way around if you use high performance gaming hardware and variable refresh rate - untested).

Note: Wine Hangover on Raspi will not work that way, as the builtin ddraw.dll is not X86 code. If you want to go this way with RasPi, you might use Box86 directly to emulate a complete X86 Build of Wine. (untested, but should be possible as Winlator on Android also does it that way and has great performance)

### Switch back to X11 (and probly encounter the next bug)

In the login screen of Ubuntu, between entering the username and password, click on the bottom right symbol and choose "Ubuntu on X.org". Within Desktop, check with echo $XDG_SESSION_TYPE to be sure the changes took effect.

## Libmutter Bug with Multiple Keyboards (X11/Gnome only)

Using Multiple Keyboards will create inacceptible slowdowns with Ubuntu/X11. Each Key Event coming from a different physical keyboard than that before (and that will happen quite often during gameplay), will cause a time consuming check of KB layout and config stuff. Events will queue up and system gets unresponsive. Check whether you are affected by connecting 2 keyboards, and high-speed-trigger keys on both KB's simultaneously while in an editor.

Same Problem here:
https://askubuntu.com/questions/1044985/using-2-keyboards-at-the-same-time-create-annoying-input-lag

Solution 1 Rebuild libmutter with patch:
https://gitlab.gnome.org/GNOME/gnome-shell/-/issues/1858#note_818548

Solution 2 Install / Use a differnet Window Manager (e.g. Openbox)

## Start Wine in Openbox (Kiosk Mode)

Start into runlevel 3 by typing 
sudo init 3
(Or configure your system to do so on boot.)

I was not able to startx wine ./Bm98.exe directly, as probably resolution change needs a Desktop Environment, but have a script that starts openbox in Kiosk mode with only Wine, highly recommended for Raspi.

Create a script b.sh with the following contents:
```
#!/bin/sh
openbox &
sleep 1
cd /path/to/AtomicBomberman
exec wine ./BM.exe
```
Now you shoud be able to:
```
startx ./b.sh
```


Before starting openbox+wine, switch to another terminal with for example LCTRL + ALT + F2 to start key2joy

## Lag every 2 seconds

Bomberman probably runs extreamly fast and fluid on any X86 PC from the last 15 years, even on Wine/Linux. Nevertheless, if at least 1 (virtual / real) joystick is connected, the game will hang slightly every 2 Seconds, for example on an i5-4200 for about 150 ms. This strange behavior has 2 different causes. Whenever at least one Joystick is present, Atomic Bomberman queries Joystick 0 to Joystick 9, regardless of whether present or not. Wine will recognize invalid queries, and itself queries the host system for an updated list of devices - in a blocking way. Doing that, Wine records a timestamp, and will trigger the next query to host 2 seconds later. As Atomic Bomberman wont be able to detect hotplugged devices anyway and needs to be restarted, this behavior of Wine brings no benefit. 

In the Directory wine_patch, you will find a patched joystick.c. Replaye original file with mine and build. In case that my instructions are outdated, just diff it to original file from Wine 10.14. 

## Joystick Name Override in Wine

In older Versions from Wine, the Vendor_ID of the joystick will be forwarded, starting with a version around Wine 7, for some reasons I dont know, that behavior changed and always defalts to "Wine joystick driver". The patch for the bug above also reverts to the old behavior, you rename your keyboard derived virtual joystick like described above and in-game, during choice of players, this name will be displayed.

## Bomberman crashing on long Joystick names (fixed)

This curious bug occurs only, when joysticks are connected. The trivial reason is that Bomberman tries to render the characters exceeding the bounds of the frame. Newer stock & patched Wine versions are not affected. Anyway I mention this just to explain why my patch truncates the joystick name to 20 chars.

There is/was a patch for Atomic Bomberman around, called Bm98.exe, that fixes this bug, but due to copyright reasons I cannot provide it. You will only need that for running it natively under newer Windows Versions.

# Building & Running Custom Wine

Building Wine has always been a challange, as well as having multiple Versions of Wine installed on a system. Using Distrobox, things have changed a bit.
Check out build instructions at:

https://gitlab.winehq.org/wine/wine/-/wikis/Distrobox-development-environment

The script from this site is also checked in here.


Clone Wine, checkout a stable branch (10.18) and patch or replace the joystick.c file. Build as described for shared WoW64 builds here. 

https://gitlab.winehq.org/wine/wine/-/wikis/Building-Wine#plain-vanilla-compiling

WARNING: NOT RECOMMENDED TO RUN THE FOLLOWING COMMANDS INSIDE VSCODE TERMINAL, FOR SOME REASON THE DISTRO IS NOT VISIBLE/AUSABLE IN STANDARD TERMINAL. It seems to store the image inside ~/snap/code/.. where it is not found.

So you might want to do something like:
```
$ sudo apt install distrobox
$ cd ProjectB/scripts
$ chmod +x distrobox_create_wine_devel.sh
$ ./distrobox_create_wine_devel.sh
$ #creating a script setup-wine-devel.sh in current directory
$ ./setup-wine-devel.sh
$ distrobox enter wine-devel
$ cd ~ #will be /home/your_username/wine-devel
$ git clone https://gitlab.winehq.org/wine/wine.git
$ cd wine
$ git checkout wine-10.19
$ cp /full_path_to/ProjectB/wine_patch/joystick.c ./dlls/winmm/joystick.c #note that you should not use ~ as your home inside distrobox differs
$ mkdir wine64-build
$ cd wine64-build
$ ../configure --enable-win64
$ make 
# or for example make -j4
$ mkdir wine32-build
$ cd wine32-build
$ PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu ../configure --with-wine64=../wine64-build
$ make
```

Now you have a Wine executable in your build directory.
As distrobox by default mounts/shares the home directory and uses the same UID, you can access the files natively and from container. 

You can even run it from the container as well as from host (if host OS is the same as the containerized OS, tested 24.04). Running inside container should work even with different systems (untested).

You then do something like:
```
cd ~/path/to/AtomicBomberman
WINEDLLOVERRIDES="ddraw.dll=n" ~/path/to/wine/wine64-build/wine ./BM.exe (on Wayland)
```

# Raspberry

You want to build the ultimate multiplayer console? Lets go! 
We will be doing a custom Wine-Hangover build for Ubuntu 25.04.

https://github.com/AndreRH/hangover

This software is a modified version of Wine, where all the Wine components are compiled for ARM64 and during runtime only the Windows Application itself is emulated through either Box86 oder FEX. Default for 32 bit exe is Box86, anyway chosing FEX crashes with Atomic Bomberman in the Wine 10.14 Build. 

Basically you can install all .deb files like instructed and should be able to start Bomberman like mentioned above.
Only one of those should be rebuild.

Cross-build instructions are a bit hidden in 
https://github.com/AndreRH/hangover/blob/master/.github/workflows/deb.yml

I Created a script that basically does the same for the specific package on Ubuntu 25.04 (make sure that you have a working Docker installation).
Copy scripts/docker_build_wine_hangover_25_04.sh to the hangover directory, replace joystick.c from the ./wine/dlls/winmm subdirectory with the one from wine_patch, and run the docker build script. (Assuming that docker run hello-world will work on your system). This script copies the deb file from the docker image to the working directory. Copy it to your RasPi and install it over the official build. (dpkg ....)

What you do is:
```
$ git clone https://github.com/AndreRH/hangover.git
$ cd hangover
$ git submodule update --init --recursive
# Checkout specific tag
$ git checkout hangover-10.18
$ cp ~/path_to/ProjectB/wine_patch/joystick.c ./wine/dlls/winmm/joystick.c
$ cp ~/path_to/ProjectB/scripts/docker_build_wine_hangover_25_04.sh .
$ ./docker_build_wine_hangover_25_04.sh
```

Note that I recommend to run it "bare metal" with just openbox, see instructions above. 

# Jsmon 

Jsmon is a command line tool intended to pre-check all joystick names and buttons. Device list should update in realtime. It will kill itself by keeping a button + axis pressed for 2 seconds and output a specific return value (depending on direction / button combo). We can catch that value by a script and have the base fore an autostart / zero-command-line-image - hopefully later. 
An example script is loop_jsmon.sh from the same directory, starting launch1up.sh, you need to adapt at least the directories from the latter one. 

```
sudo apt install libncurses5-dev libncursesw5-dev
cd jsmon
make
./loop_jsmon.sh
```

# Web2Joy (Beta)

Not enough keyboards / USB Hubs, but some laptops around? No need to install any software, a Webbrowser is all you need.

At host-side all you need is Python and PIP to install some runtime dependencies, all client systems just need to be in the same subnetwork and have to provide a webbrowser and keyboard. 

I recommend using venv. 
```
sudo chmod 777 /dev/uinput # or do it the proper way, but I dont care
sudo apt install python3-pip
sudo apt install python3.13-venv
cd web2joy
python3 -m venv .venv
./venv/bin/pip3 install flask-sock evdev
./venv/bin/python3 ./app.py
```

It will print the IP to the command line.

On your client, use a webbrowser to connect to http://IP.AD.DR.ESS:5000/stick (dont forget to add stick). Under the hood, in your client Browser a Javascrip will open a Websocket connection to the server and transmit key events. Depending on your connection, latency is somewhere between unrecognizable and "still playable". Keys are fixed to Arrow keys and 1,2 for buttons. Sometimes an axes might get stuck, pressing and releasing all directions shold recover. 


# Additional Information

## Background

In a time, when a friend and I were supposed to write our thesis', we improved our Bomberman skills to a level we were par. We came to the situation where the keyboard player always won against the gamepad player. The idea was born to connect two or more keyboards to one PC. 

## Facts 

You might have heared of the Hi-Ten Bomberman, a 10 Player Bomberman version for (Japanese early 90s) analogue high resolution TV's. It was only used for demonstration purposes at fairs and events.
At time of writing (2025/10), the wikipedia article about Battle Royale Gaming states that the 2000 Japanese film "Battle Royale" was the origin of that term. I disagree, as in a 1997 video of Hi-Ten Bomberman the gaming mode was already called that way, and it provides all the typical principles like looting, last man standing and shrinking level.

Its said that Saturn Bomberman comes quite close to Hi-Ten Bomberman. They have implemented an anamorphic widescreen mode, in a way that when you connect the console to a 16:9 CRT and stretch the 4:3 image, everything will be displayed with correct aspect ratio. Should make no difficulties to play it with 10 keyboards...but needs a halfway recent PC.

## Future Options

Depending on time and motivation, I consider working on:

* Minimal, fast booting and autostart-enabled (live) distro, targeting also those Futro-like boxes they throw out on Ebay for 10-20 bucks.
* Minimal, fast booting and autostart-enabled RaspiImage
* Fork of Winlator with multiple key2joy instances and patched Wine
* Fork of Retroarch with key2joy (10 player Saturn Bomberman)
* Compare Wine-Hangover with emulation the complete x86 build of standard Wine

## Other implementations

Here is a project to use a RasPi connected to a Windows machine via USB, where the RasPi acts as USB client and provides 10 joysticks to windows. The keyboards are connected to the RasPi.
https://github.com/benadler/keystick

A complete reimplementation of Atomic Bomberman (Original Game Contets needed) can be found here:
https://github.com/PascalCorpsman/fpc_atomic

