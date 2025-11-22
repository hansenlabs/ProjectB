#!/bin/sh

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

while true
do
    # Cursor-Blinken deaktivieren
    printf '\033[?12l'
    printf '\033[4 q'

    # Programm A starten
    # sleep 2
    "$SCRIPT_DIR/jsmon"
    ret=$?
    clear
    echo $ret
    # sleep 2
    # Prüfen, ob Rückgabewert 40–47 ist
    case "$ret" in
        10) echo "A lieferte 10 → Starte B"; $SCRIPT_DIR/launch1up.sh ;;
        11) echo "A lieferte 11 → Starte C"; sleep 1 ;;
        12) echo "A lieferte 12 → Starte E"; sleep 1 ;;
        13) echo "A lieferte 13 → Starte F"; sleep 1 ;;
        14) echo "A lieferte 14 → Starte G"; sleep 1 ;;
        15) echo "A lieferte 15 → Starte H"; sleep 1 ;;
        16) echo "A lieferte 16 → Starte I"; sleep 1 ;;
        17)
            echo "A lieferte 47 → Skript beendet."
            printf '\033[0 q'
            exit 0
            ;;
        *)
            echo "Unerwarteter Rückgabewert von A: $ret"
            sleep 5
            # optional: weiter, oder exit
            continue
            ;;
    esac

    # danach Loop → A wird wieder gestartet
done
