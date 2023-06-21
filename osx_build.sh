#!/usr/bin/env bash

# git submodule update --init --recursive

uid=$((id -u))
if [ $uid != 0 ]; then
    echo You will need elevated privilege to install the dependencies
    sudo -s
fi

submodule_path="./modules"
for f in `ls $submodule_path`; do
    cmake $submodule_path/$f -B $submodule_path/build
    sudo cmake --build $submodule_path/build --target install --parallel 4
    rm -rf $submodule_path/build
done

./autogen.sh
./configure --enable-utf8proc
sudo make -j 4
