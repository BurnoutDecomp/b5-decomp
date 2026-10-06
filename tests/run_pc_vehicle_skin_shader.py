"""Read actual installed vehicle VS and observe its skin gather on native D3D9.

The pixel shader only exposes interpolators from the unchanged vehicle vertex
shader. The production external constant dispatcher publishes all128 rows.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile
from fxgs_common import Tree, definition, compile_and_run, report, REPO, WORKFLOW

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--palette-shift", action="store_true")
parser.add_argument("--bundle", type=Path, default=WORKFLOW / "build/game/SHADERS.BNDL")
args = parser.parse_args()
source = Tree(None).read("src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp")
block = definition(source, "void DispatchExternalBlock(")
if args.palette_shift:
    block = block.replace("lpHandle[0], lpSource", "lpHandle[0] + 1u, lpSource")
methods = "namespace renderengine {\n" + definition(Tree(None).read(
    "src/pc/gcm/renderengine/XenonD3D9Shims.cpp"), "void WorldShaderConstants_Set(") + "\n}\n"
methods += block + "\n"
with tempfile.TemporaryDirectory(prefix="brn_skin_shader_") as directory:
    extracted = Path(directory) / "bundle"
    subprocess.run([str(WORKFLOW / "build/tools/yap/YAP.exe"), "e", str(args.bundle), str(extracted)],
                   stdout=subprocess.DEVNULL, check=True)
    primary = (extracted / "ShaderProgramBuffer/274C49FB_header.dat").read_bytes()
    size = struct.unpack_from("<I", primary, 8)[0]
    code = primary[20:20+size]
    assert struct.unpack_from("<I", code)[0] == 0xFFFE0300
    methods += "static const unsigned char skinVS[] = {" + ",".join(str(b) for b in code) + "};\n"
    result = compile_and_run(Path(__file__).with_name("PCVehicleSkinShader.cpp"),
        "skin_shader.inc", methods, "PCVehicleSkinShader",
        extra_flags="d3d9.lib d3dcompiler.lib user32.lib")
raise SystemExit(report("run_pc_vehicle_skin_shader", [], result, 1))
