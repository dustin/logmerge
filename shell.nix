with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "logmerge";
  buildInputs = [
    gnumake boost
  ];
}
