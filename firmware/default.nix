{
  lib,
  stdenvNoCC,
  version,
  system,
  ...
}:

stdenvNoCC.mkDerivation {
  pname = "intel-npu-firmware";
  version = version;

  src = with lib.fileset; toSource {
    root = ../.;
    fileset = unions [
      ./bin
      ../LICENSE.md
    ];
  };

  meta = {
    homepage = "https://github.com/intel/linux-npu-driver";
    description = "Intel NPU (Neural Processing Unit) firmware";
    platforms = [ system ];
    license = lib.licenses.mit;
  };

  installPhase = ''
    mkdir -p $out/lib/firmware/intel/vpu
    cp -P firmware/bin/*.bin $out/lib/firmware/intel/vpu

    mkdir -p $out/share/doc/intel-npu-driver
    cp LICENSE.md $out/share/doc/intel-npu-driver/LICENSE
  '';
}
