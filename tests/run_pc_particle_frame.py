"""Exercise native particle frame publication against actual spawn-ring writes."""
from pathlib import Path
import os
import sys
import tempfile
from fxgs_common import REPO, compile_and_run, definition, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
with tempfile.TemporaryDirectory(prefix="brn_particle_frame_") as directory:
    shadow = None
    if "--shared-banks" in sys.argv:
        path = "src/GameSource/Effects/Particles/Native/ParticleFramePC.h"
        source = (REPO / path).read_text(encoding="utf-8")
        source = source.replace("        lrBank.mpaParticles = lrStorage.data();", "        // negative: retain live bank pointer")
        shadow = {path: source}
    spawn = definition((REPO / "src/GameSource/Effects/Particles/Native/BrnSimpleParticleArray.cpp").read_text(encoding="utf-8"),
                       "bool BrnSimpleParticleArray::SpawnParticle(")
    result = compile_and_run(Path(__file__).with_name("PCParticleFrame.cpp"), "spawn_particle.inc", spawn,
        "PCParticleFrame", shadow=shadow, extra_flags="/Gy /Gw",
        extra_sources=[REPO / "src/GameShared/GameClasses/Numeric/CgsRandom.cpp"])
raise SystemExit(report("run_pc_particle_frame", [], result, 9))
