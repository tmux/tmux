{ pkgs, ghosttyVT }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    autoconf
    automake
    bison
    pkg-config
    zig
  ] ++ lib.optionals stdenv.hostPlatform.isLinux [
    # For `zig build valgrind`; not usefully supported on Darwin.
    valgrind
  ];

  buildInputs = with pkgs; [
    ghosttyVT
    ghosttyVT.dev
    libevent
    ncurses
    utf8proc
  ] ++ lib.optionals stdenv.hostPlatform.isDarwin [
    libiconv
  ];
}
