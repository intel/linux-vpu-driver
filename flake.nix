{
  description = "Intel® NPU driver";

  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  } @ inputs: let
    system = "x86_64-linux";
    lib = nixpkgs.lib;
    pkgs = import nixpkgs { system = system; };

    version = with builtins; let
      lines = lib.splitString "\n" (readFile ./CMakeLists.txt);
      extract = match "set\\(STACK_VERSION ([\\.0-9]+) .+\\)";
    in elemAt (
      lib.findFirst isList null (map extract lines)
    ) 0;
  in {
    packages.${system} = rec {
      firmware = pkgs.callPackage ./firmware/default.nix {
        inherit system version;
      };
      default = firmware;
    };
  };
}
