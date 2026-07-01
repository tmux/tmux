{
  description = "tmux fork development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    ghostty.url = "github:ghostty-org/ghostty";
  };

  outputs = { self, nixpkgs, ghostty }:
    let
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "aarch64-linux"
        "x86_64-linux"
      ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          let
            pkgs = import nixpkgs { inherit system; };
            ghosttyVT = ghostty.packages.${system}.libghostty-vt-releasefast;
          in
          f { inherit pkgs system ghosttyVT; });
    in {
      packages = forAllSystems ({ pkgs, ghosttyVT, ... }: {
        default = pkgs.stdenv.mkDerivation {
          pname = "tmux";
          version = "next-3.7";

          src = ./.;

          strictDeps = true;
          enableParallelBuilding = true;

          postPatch = ''
            mkdir -p etc
          '';

          nativeBuildInputs = with pkgs; [
            autoreconfHook
            bison
            pkg-config
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

          configureFlags = [
            "--enable-utf8proc"
          ];

          meta = with pkgs.lib; {
            description = "Terminal multiplexer with Ghostty VT integration";
            homepage = "https://github.com/tmux/tmux";
            license = licenses.isc;
            platforms = platforms.unix;
            mainProgram = "tmux";
          };
        };
      });

      apps = forAllSystems ({ system, ... }: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/tmux";
        };
      });

      checks = forAllSystems ({ system, ... }: {
        default = self.packages.${system}.default;
      });

      devShells = forAllSystems ({ pkgs, ghosttyVT, ... }: {
        default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            autoconf
            automake
            bison
            pkg-config
            zig
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
        };
      });
    };
}
