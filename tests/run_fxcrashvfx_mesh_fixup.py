"""FX-CRASHVFX (crash parity 2026-09-25, C3 = CC-15): THE DEBRIS MESH COLLECTIONS GET FIXED UP --
BrnParticle::BrnVFXMeshCollectionResourceType::FixUp @0x82678490 over the ported PARTICLES.BUNDLE collections, the
graphics lane it adds to the buffer base words, and the FLAG PC platform leaf that refuses a pre-port bundle.

The console's FixUp rebases a collection's two offsets and its MeshHelper's two buffer offsets by the header's load
address (rw lane 0, `lwz r29, 0(r30)` 0x826784A8) and adds the BODY's load address to the vertex and index buffer
base words (`lwz r9 / r10, 8(r30)` 0x82678544 / 0x82678568 -- 4-byte lanes, so lane 2, where Pool::FixUpEntry puts
the graphics block). The PC FixUp read lane 1 -- always empty -- so every buffer kept its body OFFSET for an address.
Nothing noticed: 0x10019 was not registered and the collections were passed through big-endian.

  1. WIRING -- FixUp adds lrResource.m_baseResources[2] (not [1]) and tests IsUnconvertedPC before it dereferences
     anything; the refusal line names PARTICLES.BUNDLE and the re-conversion command; LoadFXBundle stage 6 asks
     IsUnconvertedPC before it binds; the parent converter PORTS the collections (no passthrough).
  2. NUMERIC -- tests/FxCrashVfxMeshFixUp.cpp runs the PRODUCTION FixUp over the three ported headers and compares
     them, byte for byte, with 0x82678490 run on emu64 over the X360 headers
     (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_meshfixup_data.py; 3 meshes x 4 checks), plus the refusal's
     single line (1 check).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_mesh_fixup.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import WORKFLOW, Tree, code_only, compile_and_run, definition, report

MESH_CPP = "src/SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.cpp"
LIFECYCLE_CPP = "src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp"
TRANSCODER = WORKFLOW / "tools/assets/bundles/particles_transcode.py"

FIXUP = "void BrnVFXMeshCollectionResourceType::FixUp("
IS_UNCONVERTED = "bool BrnVFXMeshCollectionResourceType::IsUnconvertedPC("
REPORT = "static void ReportUnconvertedPC("
ENUMS = ("enum EVFXMeshCollectionDword", "enum EMeshHelperDword")

NUMERIC_CHECKS = 3 * 4 + 1


class RootTree(Tree):
    def __init__(self, rev=None, root=None):
        super().__init__(rev)
        self.root = Path(root) if root else None

    def read(self, relative):
        if self.root is not None and (self.root / relative).exists():
            return (self.root / relative).read_text(encoding="utf-8-sig").replace("\r\n", "\n")
        try:
            return super().read(relative).replace("\r\n", "\n")
        except FileNotFoundError:
            return ""


def body(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return ""


def wiring(tree):
    source = tree.read(MESH_CPP)
    fixup = code_only(body(source, FIXUP))
    first_deref = fixup.find("lpHeader[E_MESHCOLLECTION")
    yield ("FixUp adds the GRAPHICS lane, lrResource.m_baseResources[2], to the buffer base words (X360 4-byte lanes: "
           "`lwz r9, 8(r30)` 0x82678544 / `lwz r10, 8(r30)` 0x82678568), never lane 1",
           "m_baseResources[2]" in fixup and "m_baseResources[1]" not in fixup)
    yield ("FixUp refuses an unconverted (big-endian) collection before it dereferences anything (FLAG PC platform "
           "leaf), and the refusal names PARTICLES.BUNDLE and the re-conversion command",
           0 <= fixup.find("IsUnconvertedPC(") < first_deref and body(source, IS_UNCONVERTED) != ""
           and "--only PARTICLES.BUNDLE" in body(source, REPORT) and "PARTICLES.BUNDLE" in body(source, REPORT))
    lifecycle = code_only(tree.read(LIFECYCLE_CPP))
    load = lifecycle[lifecycle.find("bool ParticleModule::LoadFXBundle("):]
    stage6 = load[load.find("case E_LOADSTAGE_WAIT_MESH_COLLECTIONS:"):load.find("case E_LOADSTAGE_ACQUIRE_MESH_TEXTURES:")]
    yield ("LoadFXBundle stage 6 does not bind a refused collection (IsUnconvertedPC before AcquireMeshCollection)",
           0 <= stage6.find("IsUnconvertedPC(") < stage6.find(".AcquireMeshCollection("))
    tool = TRANSCODER.read_text(encoding="utf-8", errors="replace") if TRANSCODER.exists() else ""
    yield ("tools/assets/bundles/particles_transcode.py PORTS the three collections (swap_vfx_mesh_collection, checked "
           "against the debris preset names), no big-endian passthrough",
           "def swap_vfx_mesh_collection(" in tool and "def check_mesh_geometry(" in tool
           and "DEBRIS_MESH_NAMES" in tool and "(passthrough BE)" not in tool)


def numeric(tree):
    source = tree.read(MESH_CPP)
    try:
        fixup = definition(source, FIXUP)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    consts = re.findall(r"^[ \t]*static const uint32_t K[A-Z]{1,2}_[A-Z0-9_]+\s*=\s*[^;]+;", source, flags=re.M)
    enums = [definition(source, e) + ";" for e in ENUMS if e in source]
    is_unconverted = body(source, IS_UNCONVERTED)
    report_fn = body(source, REPORT)
    config = "#define FXMF_HAS_IS_UNCONVERTED %d\n" % (1 if is_unconverted else 0)
    if not is_unconverted:
        print("NUMERIC: this revision has no IsUnconvertedPC; its FixUp runs alone")
    text = "\n".join([c.strip() for c in consts] + enums + [report_fn, is_unconverted, fixup]) + "\n"
    return compile_and_run(Path(__file__).with_name("FxCrashVfxMeshFixUp.cpp"), "fxcrashvfx_meshfixup_body.inc", text,
                           "FxCrashVfxMeshFixUp", extra_files={"fxcrashvfx_meshfixup_config.inc": config})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_mesh_fixup", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
