{
  description = "Minimal Wayland autoclicker using uinput";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAll = f: nixpkgs.lib.genAttrs systems (s: f nixpkgs.legacyPackages.${s});
    in {
      packages = forAll (pkgs: rec {
        autoclicker = pkgs.stdenv.mkDerivation {
          pname = "autoclicker";
          version = "0.1";
          src = self;
          makeFlags = [ "PREFIX=${placeholder "out"}" ];
          meta.mainProgram = "autoclicker";
        };
        default = autoclicker;
      });

      overlays.default = final: _: {
        autoclicker = self.packages.${final.system}.default;
      };
    };
}
