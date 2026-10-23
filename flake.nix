{
  description = "Shelf — glass file manager for Hyprland in Qt 6 / QML";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # nix run github:Stasyao37/shelf — запустить, не устанавливая
      # nix profile install github:Stasyao37/shelf — поставить в профиль
      packages = forAllSystems (pkgs: rec {
        shelf = pkgs.callPackage ./packaging/nix/package.nix { };
        default = shelf;
      });

      # Для NixOS: nixpkgs.overlays = [ shelf.overlays.default ]; и pkgs.shelf в systemPackages
      overlays.default = final: prev: {
        shelf = final.callPackage ./packaging/nix/package.nix { };
      };

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
