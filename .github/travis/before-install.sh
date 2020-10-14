#!/bin/sh

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
	sudo apt-get update -qq
	sudo apt-get -y install bison \
				autotools-dev \
				libncurses5-dev \
				libevent-dev \
				pkg-config \
				libutempter-dev \
				build-essential

	if [ "$BUILD" = "musl" -o "$BUILD" = "musl-static" ]; then
		sudo apt-get -y install musl-dev \
					musl-tools
	fi
fi

if [ "$TRAVIS_OS_NAME" = "freebsd" ]; then
	sudo pkg install -y \
		automake \
		libevent \
		pkgconf
fi


if [ "$BUILD" = "musl" -o "$BUILD" = "musl-static" ]; then
	IS_MUSL=yes
else
	IS_MUSL=no
fi

case "$TRAVIS_CPU_ARCH" in
	amd64)
		RUST_ARCH=x86_64
		;;
	arm64)
		RUST_ARCH=aarch64
		;;
esac

case "$TRAVIS_OS_NAME" in
	linux)
		if [ "${IS_MUSL}" = yes ]; then
			RUST_TARGET=${RUST_ARCH}-unknown-linux-musl
		else
			RUST_TARGET=${RUST_ARCH}-unknown-linux-gnu
		fi
		;;
	freebsd)
		if [ "${IS_MUSL}" = yes ]; then
			echo "musl target for FreeBSD is not supported" >&2
			exit 1
		else
			RUST_TARGET=${RUST_ARCH}-unknown-freebsd
		fi
		;;
	osx)
		if [ "${IS_MUSL}" = yes ]; then
			echo "musl target for Mac is not supported" >&2
			exit 1
		else
			RUST_TARGET=${RUST_ARCH}-apple-darwin
		fi
		;;
esac

curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s - --target ${RUST_TARGET} --profile minimal -y
mkdir -p ${HOME}/.cargo/
cat <<END > ${HOME}/.cargo/config.toml
[build]
rustflags = ["--target", "${RUST_TARGET}"]
END
