{
  description = "Logmerge";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = { self, nixpkgs }: 
    let
      eachSystem = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
    in {
      packages = eachSystem (system: let pkgs = import nixpkgs { inherit system; }; in {
        default = pkgs.stdenv.mkDerivation {
          name = "logmerge";
          src = ./.;
          buildInputs = [ pkgs.zig ];
        };
      });

      devShells = eachSystem (system: let pkgs = import nixpkgs { inherit system; }; in {
        default = pkgs.mkShell {
          buildInputs = [ pkgs.gnumake pkgs.boost pkgs.zig ];
        };
      });
    };
}
