"""Real bloom producer binds must replace prior world sampler state on unit0.

Extract all four production bind sites and their native applier. The unchanged
downsample and blur pixel programs consume moving edge samples at720p/1440p.
--rev runs the earlier producer on the same device as a regression control.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rev")
args = parser.parse_args()
tree = Tree(args.rev)
bloom = tree.read("src/GameSource/Graphics/PostFx/BrnPostFxBloom.cpp")
shim = tree.read("src/pc/gcm/renderengine/XenonD3D9Shims.cpp")
shadow = tree.read("src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp")
methods = definition(shim, "void ApplyPostFxSourceSamplerState(") + "\n"
methods += 'extern "C" {\n' + definition(shim, "void* SetSamplerStateLowLevel(") + "\n}\n"
methods += "namespace shadow {\n" + definition(shadow,
    "void* Device::SetState(const renderengine::TextureState* lpState,") + "\n}\n"
methods += "namespace renderengine {\n"
methods += definition(shim, "void PostFxSourceSampler_ApplyState(") + "\n"
try:
    methods += definition(shim, "void PostFxBloomSampler_ApplyState(") + "\n"
except ValueError:
    # The baseline has no producer applier and no call to one. Its original
    # texture bind still compiles and is exercised without adding a fallback.
    pass
methods += "}\n"
binds = []
for name in ("PrepareDownSampleBuffer", "Generate1PassBlurredBloomBuffer",
             "Generate2PassBlurredBloomBuffer"):
    body = definition(bloom, f"void BrnPostFxBloom::{name}(")
    for match in re.finditer(r"    shadow::Device::SetState\(lp(?:Source|Work|Bloom)Rt->"
                            r"maColourTargets\[0\]\.mpTextureState, KU_SAMPLER_SOURCE\);", body):
        end = body.find("\n", match.end())
        next_line = body[match.end():end + 1]
        # The optional PC realization is the next complete statement. Preserve
        # it verbatim when present, rather than recreating the behavior in tests.
        following_end = body.find("\n", end + 1)
        following = body[end + 1:following_end]
        block = match[0] + next_line
        if "renderengine::PostFxBloomSampler_ApplyState(" in following:
            block += following + "\n"
        index = len(binds)
        binds.append(f"void BindBloomSource{index}(TargetFixture* lpSourceRt, "
                     "TargetFixture* lpWorkRt, TargetFixture* lpBloomRt) {\n" + block + "}\n")
if len(binds) != 4:
    raise SystemExit(f"Expected four bloom producer binds, found {len(binds)}")
methods += "\n".join(binds)
numeric = compile_and_run(Path(__file__).with_name("PCBloomSourceSampler.cpp"),
    "bloom_source_sampler.inc", methods, "PCBloomSourceSampler",
    extra_sources=[REPO / "src/pc/gcm/renderengine/PostFxBloomProgramsPC.cpp"],
    extra_flags="/Gy /Gw d3d9.lib user32.lib")
raise SystemExit(report("run_pc_bloom_source_sampler", [], numeric, 1))
