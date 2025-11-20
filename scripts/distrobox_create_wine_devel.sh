DISTROBOX_NAME=wine-devel
DISTROBOX_HOME=$HOME/$DISTROBOX_NAME
mkdir -p $DISTROBOX_HOME # Distrobox will create the container's HOME directory anyway, but create it beforehand to store `devenv-init.sh`

# Host
DISTROBOX_ASSEMBLE_SCRIPT=setup-$DISTROBOX_NAME.sh
DISTROBOX_MANIFEST=$DISTROBOX_NAME.ini

# Container
DEVENV_INIT_SCRIPT=$DISTROBOX_HOME/devenv-init.sh

cat << EOF > setup-$DISTROBOX_NAME.sh
#! /bin/sh
DISTROBOX_NAME=$DISTROBOX_NAME
DISTROBOX_MANIFEST=$DISTROBOX_MANIFEST

export DISTROBOX_HOME=$DISTROBOX_HOME

ADD_PKGS_BASE="git gdb ccache valgrind"
ADD_PKGS_UTILS="vim tmux meld icecc"
ADD_PKGS_WINE_DEVEL_DEPS="make \\
                          flex \\
                          bison \\
                          gcc-multilib \\
                          gcc-mingw-w64 \\
                          libglib2.0-dev libglib2.0-dev:i386 \\
                          libasound2-dev libasound2-dev:i386 \\
                          bluez \\
                          libpulse-dev libpulse-dev:i386 \\
                          libdbus-1-dev libdbus-1-dev:i386 \\
                          libfontconfig-dev libfontconfig-dev:i386 \\
                          libfreetype-dev libfreetype-dev:i386 \\
                          libgnutls28-dev libgnutls28-dev:i386 \\
                          libgl-dev libgl-dev:i386 \\
                          libunwind-dev libunwind-dev:i386 \\
                          libx11-dev libx11-dev:i386 \\
                          libxcomposite-dev libxcomposite-dev:i386 \\
                          libxcursor-dev libxcursor-dev:i386 \\
                          libxfixes-dev libxfixes-dev:i386 \\
                          libxi-dev libxi-dev:i386 \\
                          libxrandr-dev libxrandr-dev:i386 \\
                          libxrender-dev libxrender-dev:i386 \\
                          libxext-dev libxext-dev:i386 \\
                          libwayland-bin \\
                          libwayland-dev libwayland-dev:i386 \\
                          libegl-dev libegl-dev:i386 \\
                          libxkbcommon-dev libxkbcommon-dev:i386 \\
                          libxkbregistry-dev libxkbregistry-dev:i386 \\
                          libgstreamer1.0-dev libgstreamer1.0-dev:i386 \\
                          libosmesa6-dev libosmesa6-dev:i386 \\
                          libsdl2-dev libsdl2-dev:i386 \\
                          libudev-dev libudev-dev:i386 \\
                          libvulkan-dev libvulkan-dev:i386 \\
                          libavformat-dev libavformat-dev:i386 \\
                          libgstreamer-plugins-base1.0-dev:i386" # libgstreamer-plugins-base1.0-dev skipped, conflicts with i386 variant - see https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1016631

export ADD_PKGS="\$ADD_PKGS_BASE \$ADD_PKGS_UTILS \$ADD_PKGS_WINE_DEVEL_DEPS"

export PRE_INIT_HOOKS="dpkg --add-architecture i386" # 32-bit dependencies

distrobox assemble create --file \$DISTROBOX_MANIFEST
EOF

cat << EOF > $DISTROBOX_MANIFEST
[$DISTROBOX_NAME]
image=ubuntu:latest # Latest LTS release
home=\$DISTROBOX_HOME
additional_packages=\$ADD_PKGS
root=false
start_now=true
pre_init_hooks=\$PRE_INIT_HOOKS
EOF

cat << EOF > $DEVENV_INIT_SCRIPT
#! /bin/sh
printf "\ncd" >> \$HOME/.\${SHELL}rc
EOF

reset

echo "1. Run the generated script \"./$DISTROBOX_ASSEMBLE_SCRIPT\" to create a distrobox container."
echo "2. Run \"distrobox enter $DISTROBOX_NAME\" to enter the container."
echo "3. Within the $DISTROBOX_NAME container, run the devenv init script with \"$DEVENV_INIT_SCRIPT\"."
