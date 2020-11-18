#!/bin/bash

# Contributed by michaellee8 <ckmichael8@gmail.com> in 2020

set -x
set -e

# Reference: https://docs.appimage.org/packaging-guide/from-source/native-binaries.html#id2

# Dependencies: sudo apt-get install pkg-config autotools automake \
#                                    autoconf curl imagemagick

# building in temporary directory to keep system clean
# use RAM disk if possible (as in: not building on CI system 
# like Travis, and RAM disk is available)
# DISABLED: It seems that linuxdeploy won't be executable on shared memory,
# maybe /dev/shm is marked as non-executable?
if [ "$CI" == "" ] && [ -d /dev/shm ] && false; then
    TEMP_BASE=/dev/shm
else
    TEMP_BASE=/tmp
fi

BUILD_DIR=$(mktemp -d -p "$TEMP_BASE" appimage-build-XXXXXX)

# make sure to clean up build dir, even if errors occur
cleanup () {
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
}
trap cleanup EXIT

# store repo root as variable
REPO_ROOT="$(git rev-parse --show-toplevel)"
OLD_CWD=$(readlink -f .)

mkdir -p "$BUILD_DIR/AppDir/usr"

cd $BUILD_DIR

# Obtain and compile libevent

git clone https://github.com/libevent/libevent --depth 1 \
    -b release-2.1.12-stable
cd libevent
sh autogen.sh
./configure \
    --prefix="$BUILD_DIR/AppDir/usr" \
    --enable-shared
make -j4
make install
cd ../

# Obtain and compile libncursesw6 so that we get 256 color support
curl -OL https://invisible-island.net/datafiles/release/ncurses.tar.gz
tar -xf ncurses.tar.gz
NCURSES_DIR="$PWD/ncurses-6.2"
pushd "$NCURSES_DIR"
./configure --with-shared --prefix="$BUILD_DIR/AppDir/usr" \
    --without-normal --without-debug
make -j4
make install
popd



# Configure tmux now to make sure it uses our libncursesw6
# and libevent
cd $REPO_ROOT
export LD_LIBRARY_PATH="$BUILD_DIR/AppDir/usr/lib"
# autoreconf -f -i
# autoconf
sh autogen.sh
export CPPFLAGS="-I$BUILD_DIR/AppDir/usr/include -I$BUILD_DIR/AppDir/usr/include/ncursesw" 
export LDFLAGS="-L$BUILD_DIR/AppDir/usr/lib"
export PKG_CONFIG_PATH=$BUILD_DIR/AppDir/usr/lib/pkgconfig
./configure \
    --prefix="$BUILD_DIR/AppDir/usr" 
make -j4
make install

# Copy the AppData file to AppDir manually
# cp -r "$REPO_ROOT/data/metainfo" "$BUILD_DIR/AppDir/usr/share/" 


# Custom AppRun to provide $ARGV0 issues when used with zsh
# Reference: https://github.com/neovim/neovim/blob/master/scripts/genappimage.sh
# Reference: https://github.com/neovim/neovim/issues/9341

cd $BUILD_DIR
cat << 'EOF' > AppDir/AppRun
#!/bin/bash
unset ARGV0
export TERMINFO=$APPDIR/usr/share/terminfo
exec "$(dirname "$(readlink  -f "${0}")")/usr/bin/tmux" ${@+"$@"}
EOF
chmod 755 AppDir/AppRun


cat << 'EOF' > AppDir/tmux.desktop
[Desktop Entry]
X-AppImage-Name=tmux
X-AppImage-Version=1.0.0
X-AppImage-Arch=x86_64
Name=Tmux
Exec=tmux
Icon=favicon
Type=Application
Categories=Utility;
EOF


# Downloading linuxdeploy

curl -o ./linuxdeploy -L \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage

chmod +rx ./linuxdeploy

# Requires imagemagick to convert favicon.ico
convert "$REPO_ROOT/logo/favicon.ico" "$REPO_ROOT/logo/favicon..png"
# favicon.ico here has multiple image size, we choose the largest one
cp "$REPO_ROOT/logo/favicon-1.png" "$BUILD_DIR/AppDir/favicon.png"

OUTPUT="tmux.appimage" ./linuxdeploy --appdir ./AppDir --output appimage \
    --icon-file "$REPO_ROOT/logo/favicon.ico" \
    --executable "$BUILD_DIR/AppDir/usr/bin/tmux"

mv "tmux.appimage" "$OLD_CWD"
