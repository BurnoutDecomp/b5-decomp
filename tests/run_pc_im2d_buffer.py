"""Record and replay production APT/FLAPT buffers on native D3D9 at three sizes."""
from pathlib import Path
import os
import sys
import tempfile
from fxgs_common import compile_and_run, report, REPO

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
here = Path(__file__).resolve().parent
im = REPO / "src/GameShared/GameClasses/Graphics/ImmediateMode"
sources = [im / "ImRenderBuffer/CgsImRenderBufferTemplate.cpp",
           im / "ImRenderBuffer/CgsIm2dRenderBuffer.cpp", im / "CgsIm2d.cpp",
           REPO / "vendor/renderware/src/rwcore_alloc.cpp",
           REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
           REPO / "vendor/coreallocator/source/icoreallocator_interface.cpp",
           REPO / "vendor/PPMalloc/src/EAGeneralAllocator.cpp"]
with tempfile.TemporaryDirectory(prefix="brn_im2d_negative_") as directory:
    if "--drop-batches" in sys.argv:
        # Negative proof: reproduce the former missing-opcode behavior while
        # retaining the current producer/ABI so this fails on pixels, not compile.
        source = sources[0].read_text(encoding="utf-8")
        needle = "            // ARTIST 0x827F9EBC..0x827F9F10:"
        assert needle in source
        source = source.replace(needle, "            continue; // negative control: old skipped batch\n" + needle, 1)
        sources[0] = Path(directory) / "CgsImRenderBufferTemplate.cpp"
        sources[0].write_text(source, encoding="utf-8")
    result = compile_and_run(here / "PCIm2dBuffer.cpp", "unused.inc", "", "PCIm2dBuffer",
                             extra_sources=sources, extra_flags="/Gy /Gw d3d9.lib user32.lib")
raise SystemExit(report("run_pc_im2d_buffer", [], result, 62))
