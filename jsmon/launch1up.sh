#!/bin/sh
#Atomic Bomberman
cd ~/Spiele/AtomicBomberman
if [ "$WAYLAND_DISPLAY" ]; then
    printf "Starte Wayland-Command...\n"
    #SW upscaling using cnc-ddraw
    WINEDLLOVERRIDES="ddraw.dll=n" ~/wine-devel/wine/wine64-build/wine ./BM.exe
elif [ "$DISPLAY" ]; then
    printf "Starte X11-Command...\n"
    #X11 will change resolution VGA, therefore no SW upscaling
    ~/wine-devel/wine/wine64-build/wine ./BM.exe
else
    echo "Keine grafische Session erkannt."
fi

