set -x
set -e

# Reference: https://docs.appimage.org/packaging-guide/from-source/native-binaries.html#id2

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

# Obtain and compile libncursesw6 so that we get 256 color support
curl -OL https://invisible-island.net/datafiles/release/ncurses.tar.gz
tar -xf ncurses.tar.gz
NCURSES_DIR="$PWD/ncurses-6.2"
pushd "$NCURSES_DIR"
./configure --with-shared --enable-widec --prefix="$BUILD_DIR/AppDir/usr" \
    --without-normal --without-debug
make -j4
make install
popd

## Configure vifm now to make sure it uses our libncursesw6
export LD_LIBRARY_PATH="$BUILD_DIR/AppDir/usr/lib"
autoreconf -f -i
autoconf
export CPPFLAGS="-I$BUILD_DIR/AppDir/usr/include -I$BUILD_DIR/AppDir/usr/include/ncursesw" 
export LDFLAGS="-L$BUILD_DIR/AppDir/usr/lib"
./configure \
    --prefix="$BUILD_DIR/AppDir/usr" 
make -j4
make install

# Copy the AppData file to AppDir manually
cp -r "$REPO_ROOT/data/metainfo" "$BUILD_DIR/AppDir/usr/share/" 


# Custom AppRun to provide $ARGV0 issues when used with zsh
# Reference: https://github.com/neovim/neovim/blob/master/scripts/genappimage.sh
# Reference: https://github.com/neovim/neovim/issues/9341

cd $BUILD_DIR
cat << 'EOF' > AppDir/AppRun
#!/bin/bash
unset ARGV0
export TERMINFO=$APPDIR/usr/share/terminfo
exec "$(dirname "$(readlink  -f "${0}")")/usr/bin/vifm" ${@+"$@"}
EOF
chmod 755 AppDir/AppRun


# Downloading linuxdeploy

curl -o ./linuxdeploy -L \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage

chmod +rx ./linuxdeploy

OUTPUT="vifm.appimage" ./linuxdeploy --appdir ./AppDir --output appimage \
    --desktop-file "$REPO_ROOT/data/vifm.desktop" --icon-file "$REPO_ROOT/data/graphics/vifm.png" \
    --executable "$BUILD_DIR/AppDir/usr/bin/vifm" --library "$BUILD_DIR/AppDir/usr/lib/libncursesw.so.6"

mv "vifm.appimage" "$OLD_CWD"
