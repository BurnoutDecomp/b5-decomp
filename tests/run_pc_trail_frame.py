"""Freeze actual trail draw inputs while the following wheel update reuses them.

--rev uses that revision's frame/publication implementation. Before the snapshot
exists, the fixture reads the live inputs exactly as TrailSystem::Render did.
"""
import argparse
import os
import subprocess
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
frame_path = "src/GameSource/Effects/Particles/Native/TrailFramePC.h"
try:
    frame = tree.read(frame_path)
except (FileNotFoundError, subprocess.CalledProcessError):
    frame = "#pragma once\n"
source = tree.read("src/GameSource/Effects/Particles/Native/BrnTrailSystem.cpp")
methods = "#define HAS_TRAIL_FRAME " + str(int("class TrailFramePC" in frame)) + "\n"
methods += "namespace BrnParticle { namespace Native {\n"
for signature in ("bool EmitterArray::Prepare(", "void EmitterArray::AddEntry(",
                  "void EmitterArray::RemoveEntry(", "TrailEmitter* EmitterArray::operator[]("):
    methods += definition(source, signature) + "\n"
methods += definition(tree.read("src/GameSource/Effects/Particles/Native/BrnTrailRender.cpp"),
                      "void TrailRenderer::Update(")
methods += "\n} }\n"
headers = {frame_path: frame}
result = compile_and_run(Path(__file__).with_name("PCTrailFrame.cpp"), "trail_frame.inc",
    methods, "PCTrailFrame", shadow=headers)
raise SystemExit(report("run_pc_trail_frame", [], result, 1))
