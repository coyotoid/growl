{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    clang-tools bear gdb tinycc
    meson ninja
  ];
}
