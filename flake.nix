{
    inputs = {
        nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.11";
        flake-utils.url = "github:numtide/flake-utils";

        fabricate.url = "github:elysium-os/fabricate";
        fabricate.inputs.nixpkgs.follows = "nixpkgs";
    };

    outputs =
        { nixpkgs, flake-utils, ... }@inputs:
        flake-utils.lib.eachDefaultSystem (
            system:
            let
                pkgs = import nixpkgs { inherit system; };
            in
            {
                devShells.default = pkgs.mkShell {
                    shellHook = "export NIX_SHELL_NAME='Freestanding Inflate Library'; export NIX_HARDENING_ENABLE='';";
                    nativeBuildInputs = with pkgs; [
                        inputs.fabricate.defaultPackage.${system}
                        ninja
                        clang
                        gdb
                        python3
                    ];
                };
            }
        );
}
