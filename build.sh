#!/bin/bash
#
# build.sh - Compilation et televersement de la station meteo WWW.
#
# Compile le programme avec le compilateur croise arm-none-eabi-gcc integre
# a PlatformIO, donne un numero de build, puis televerse le firmware sur la
# carte STM32 Nucleo L476RG par le ST-LINK.
#
# Usage :
#   ./build.sh [debug|release] [options]
#
# Modes :
#   debug          LOG_INTERVAL en secondes (tests, demonstration), -Og -g3
#   release        LOG_INTERVAL en minutes (version livree), -Os (par defaut)
#
# Options :
#   upload=o|n     repond d'avance a la question du televersement
#   clean          supprime les fichiers de compilation puis quitte
#   help           affiche cette aide
#
# Exemples : ./build.sh release
#            ./build.sh debug upload=o

##############
# FUNCTIONS  #
##############

# fail : affiche une erreur et arrete le script avec un code d'erreur
function fail() {
    printf '\n[ERREUR] %s\n' "$1" >&2
    exit 1
}

# usage : affiche l'aide (les lignes de commentaire du debut du script)
function usage() {
    sed -n '3,22p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

# trouverPio : la commande pio du terminal peut etre absente ou trop ancienne
# (apt installe la 4.3.4, cassee) : on prefere celle installee par VS Code
function trouverPio() {
    for p in "$HOME/.platformio/penv/bin/pio" "$HOME/.platformio/penv/Scripts/pio.exe"; do
        if [ -x "$p" ]; then
            echo "$p"
            return
        fi
    done
    command -v pio
}

# trouverLecteurNucleo : la carte apparait aussi comme une cle USB (NOD_L476RG).
# Copier le .bin dessus suffit a la programmer, sans pilote ST-LINK.
function trouverLecteurNucleo() {
    # Linux : /media/<utilisateur>/NOD_L476RG ; Windows (Git Bash) : une lettre de lecteur, /d, /e...
    for d in /media/"$USER"/NOD_* /run/media/"$USER"/NOD_* /{d..z}; do
        if [ -f "$d/MBED.HTM" ] && [ -f "$d/DETAILS.TXT" ]; then
            echo "$d"
            return
        fi
    done
}

###############
# ARGUMENTS   #
###############

MODE="release"
UPLOAD=""
CLEAN=0

for i in "$@"; do
    case $i in
        debug|release)  MODE="$i" ;;
        upload=*)       UPLOAD="${i#upload=}" ;;
        clean)          CLEAN=1 ;;
        help|-h|--help) usage ;;
        *)              fail "argument inconnu : $i (voir ./build.sh help)" ;;
    esac
done

printf "####################\n"
printf "#   STATION  WWW   #\n"
printf "####################\n"

# Le script se lance depuis n'importe ou : on se place dans le dossier du projet
cd "$(dirname "$0")" || fail "impossible d'entrer dans le dossier du projet"
[ -f platformio.ini ] || fail "platformio.ini introuvable a cote du script"

PIO=$(trouverPio)
[ -n "$PIO" ] || fail "PlatformIO introuvable. Installer l'extension PlatformIO de VS Code"

if [ $CLEAN == 1 ]; then
    "$PIO" run -t clean -e debug > /dev/null
    "$PIO" run -t clean -e release > /dev/null
    printf "Fichiers de compilation supprimes\n"
    exit 0
fi

#####################################
# NUMERO DE BUILD
#####################################

# Le numero est garde dans .build_number et augmente a chaque compilation.
# Il est passe au code C avec -DNUMERO_BUILD=<n> (affiche par la commande VERSION).
BUILD=0
[ -f .build_number ] && BUILD=$(cat .build_number)
BUILD=$((BUILD + 1))
echo $BUILD > .build_number

printf "Build n°%s | mode %s\n" "$BUILD" "$MODE"

#####################################
# COMPILATION
#####################################

# Les options du mode (debug / release) sont dans platformio.ini, sections
# [env:debug] et [env:release]. PLATFORMIO_BUILD_FLAGS ajoute le numero de build.
export PLATFORMIO_BUILD_FLAGS="-DNUMERO_BUILD=$BUILD"

LOG=".pio/build_$MODE.log"
mkdir -p .pio
if ! "$PIO" run -e "$MODE" > "$LOG" 2>&1; then
    cat "$LOG" >&2
    fail "la compilation a echoue (detail ci-dessus)"
fi

# Taille du programme : Flash = code + constantes, RAM = variables
grep -E "^(RAM|Flash):" "$LOG"

FIRMWARE=".pio/build/$MODE/firmware.bin"
[ -f "$FIRMWARE" ] || fail "$FIRMWARE introuvable apres la compilation"
printf "Compilation terminee : %s\n" "$FIRMWARE"

#####################################
# TELEVERSEMENT VIA ST-LINK
#####################################

if [ "$UPLOAD" == "" ]; then
    printf "Voulez vous commencer le televersement ? [O/n]: "
    read -r UPLOAD
fi

if [[ "$UPLOAD" == "n" || "$UPLOAD" == "N" ]]; then
    printf "Bye bye\n"
    exit 0
fi

if command -v st-flash > /dev/null && st-info --probe 2> /dev/null | grep -q "chipid"; then
    # Linux avec stlink-tools : ecrit le .bin au debut de la Flash (0x08000000) puis redemarre
    printf "Televersement avec st-flash\n"
    st-flash --reset write "$FIRMWARE" 0x08000000 || fail "televersement echoue"
else
    # Sinon : copie sur le lecteur NOD_L476RG, le ST-LINK programme la carte tout seul
    LECTEUR=$(trouverLecteurNucleo)
    [ -n "$LECTEUR" ] || fail "aucune carte detectee (USB branche ? redirige vers la VM ?)"
    printf "Televersement par copie sur %s\n" "$LECTEUR"
    cp "$FIRMWARE" "$LECTEUR/" || fail "copie sur la carte echouee"
    sync
fi

printf "Televersement termine : build n°%s (%s) sur la carte.\n" "$BUILD" "$MODE"
exit 0
