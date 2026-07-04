{
  description = "tmux fork development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    ghostty = {
      url = "github:ghostty-org/ghostty";
      inputs.nixpkgs.follows = "nixpkgs";
    };
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

          dontConfigure = true;

          buildPhase = ''
            runHook preBuild
            export ZIG_GLOBAL_CACHE_DIR=$PWD/zig-pkg
            export ZIG_LOCAL_CACHE_DIR=$TMPDIR/zig-cache
            zig build -Dghostty-vt=true -Dutf8proc=true -Doptimize=ReleaseFast
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            export ZIG_GLOBAL_CACHE_DIR=$PWD/zig-pkg
            export ZIG_LOCAL_CACHE_DIR=$TMPDIR/zig-cache
            zig build -Dghostty-vt=true -Dutf8proc=true -Doptimize=ReleaseFast --prefix $out install
            runHook postInstall
          '';

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
