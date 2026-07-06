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
            # Vendored Zig package cache. Zig fetches `build.zig.zon` dependencies
            # over the network, which the Nix build sandbox forbids. We instead
            # pre-fetch them as fixed-output derivations and lay them out as a
            # Zig `--system` package directory: one entry per dependency, named by
            # its `build.zig.zon` hash. Passing `--system ${zigDeps}` makes
            # `zig build` resolve deps from this directory instead of the network.
            zigDeps = pkgs.linkFarm "tmux-zig-deps" [
              {
                # google/wuffs, mirrored by ghostty (see build.zig.zon)
                name = "N-V-__8AAAzZywE3s51XfsLbP9eyEw57ae9swYB9aGB6fCMs";
                path = pkgs.fetchzip {
                  url = "https://deps.files.ghostty.org/wuffs-122037b39d577ec2db3fd7b2130e7b69ef6cc1807d68607a7c232c958315d381b5cd.tar.gz";
                  hash = "sha256-XbupK4QYnPudUlO5tRWrQRncGHITzJL//Yk/E7WNxYk=";
                };
              }
            ];
          in
          f { inherit pkgs system ghosttyVT zigDeps; });
    in {
      packages = forAllSystems ({ pkgs, ghosttyVT, zigDeps, ... }: {
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
            # The zig setup hook drives the configure/build/install phases and
            # points ZIG_GLOBAL_CACHE_DIR at a writable temp dir for us.
            zig.hook
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

          # Opt out of the hook's default -Dcpu/--release flags so we can pick
          # ReleaseFast, and skip its check phase (build.zig defines no `test`
          # step). `--system ${zigDeps}` resolves dependencies from the vendored
          # cache instead of the network, which the build sandbox blocks.
          dontSetZigDefaultFlags = true;
          dontUseZigCheck = true;

          zigBuildFlags = [
            "--system"
            "${zigDeps}"
            "-Dghostty-vt=true"
            "-Dutf8proc=true"
            "-Doptimize=ReleaseFast"
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
