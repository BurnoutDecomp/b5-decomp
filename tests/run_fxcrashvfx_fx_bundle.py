"""FX-CRASHVFX (crash parity 2026-09-25, C3 = CC-15): THE DEBRIS MESHES' LOAD LADDER -- ParticleModule::LoadFXBundle
@0x8229C950 stages 5..8 (0x8229CEB4..0x8229D1A4).

What the console does at boot, between the particle descriptions (stage 10) and the effect textures (stage 11):
    5  one acquire per debris array, pool 13, for its preset's mesh collection (lowres_debris.rf3 x3,
       highres_debris_02.rf3, Glass_debris.rf3), event id = the running request count;
    6  wait for five replies, then hand each AcquireResourceResponse's handle to maDebris[event id]
       (AcquireMeshCollection) -- no event-id check;
    7  one acquire per array for the texture its collection names (+0x90: WHITE x3, highres_debris, glass_debris);
    8  wait, then each reply's handle to maDebris[event id].mTexture (AcquireTexture), and on to stage 11.
Before C3 the PC switch went 10 -> 11: the five arrays never got a mesh or a texture, and every debris array was
skipped at draw time ("[debrispass] array SKIPPED: mMeshCollection is null").

  1. WIRING -- the four cases sit between WAIT_DESCRIPTIONS and ACQUIRE_TEXTURES in the switch, with the bodies above;
     the banner no longer says the ladder skips them; BrnDebrisArray declares the three DWARF members
     (BrnDebrisRenderer.h:144 / :151 / :157); CgsResourceTypeRegistration.cpp registers
     BrnVFXMeshCollectionResourceType (0x10019); the parent build mounts its TU.
  2. NUMERIC -- tests/FxCrashVfxFxBundle.cpp compiles the PRODUCTION LoadFXBundle (+ the revision's BrnDebrisArray
     and its GetTextureName) over the real receiver queue and compares every call against 0x8229C950 run on emu64
     (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_fxbundle_data.py; 8 cases, 21 calls x 4 checks), plus the PC
     leaf's stale-bundle contract (2 checks).

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxcrashvfx_fx_bundle.py
                                                                   [--rev <b5 rev>] [--root <shadow tree root>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, WORKFLOW, Tree, code_only, compile_and_run, definition, report

LIFECYCLE_CPP = "src/GameSource/Effects/Particles/ParticleModule_Lifecycle.cpp"
ARRAY_CPP = "src/GameSource/Effects/Particles/Native/BrnDebrisArray.cpp"
ARRAY_H = "src/GameSource/Effects/Particles/Native/BrnDebrisArray.h"
REGISTRATION_CPP = "src/GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.cpp"
QUEUE_CPP = REPO / "src/GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.cpp"
BUILD_BAT = WORKFLOW / "tools/build/build_game_exe.bat"
SHADOW_HEADERS = (ARRAY_H,)

LOAD_FX_BUNDLE = "bool ParticleModule::LoadFXBundle("
NEXT_REPLY = "const AcquireResponse* NextAcquireResponse("
GET_TEXTURE_NAME = "const char* BrnDebrisArray::GetTextureName("

NUMERIC_CHECKS = 21 * 4 + 2


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


def segment(text, start, end):
    first = text.find(start)
    if first < 0:
        return ""
    last = text.find(end, first + len(start))
    return text[first:last] if last > first else ""


def constants_for(source, text):
    """Every `const <type> K*_NAME = ...;` the text names, transitively, in source order."""
    seen, found = set(), {}
    pending = set(re.findall(r"\bK[A-Z]{1,3}_[A-Z0-9_]+\b", code_only(text)))
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        match = re.search(r"^[ \t]*const\s+[\w:]+\s*\*?\s*%s\s*=\s*[^;]+;" % re.escape(name), source, flags=re.M)
        if match:
            found[name] = (match.start(), match.group(0).strip())
            pending |= set(re.findall(r"\bK[A-Z]{1,3}_[A-Z0-9_]+\b", code_only(match.group(0)))) - seen
    return [found[name][1] for name in sorted(found, key=lambda n: found[n][0])]


def wiring(tree):
    lifecycle = tree.read(LIFECYCLE_CPP)
    ladder = code_only(body(lifecycle, LOAD_FX_BUNDLE))
    order = ["case E_LOADSTAGE_WAIT_DESCRIPTIONS:", "case E_LOADSTAGE_ACQUIRE_MESH_COLLECTIONS:",
             "case E_LOADSTAGE_WAIT_MESH_COLLECTIONS:", "case E_LOADSTAGE_ACQUIRE_MESH_TEXTURES:",
             "case E_LOADSTAGE_WAIT_MESH_TEXTURES:", "case E_LOADSTAGE_ACQUIRE_TEXTURES:"]
    positions = [ladder.find(label) for label in order]
    stage5 = segment(ladder, order[1], order[2])
    stage6 = segment(ladder, order[2], order[3])
    stage7 = segment(ladder, order[3], order[4])
    stage8 = segment(ladder, order[4], order[5])
    yield ("LoadFXBundle's switch runs 10 -> 5 -> 6 -> 7 -> 8 -> 11 (0x8229CEB4, 0x8229D1A8): the four mesh cases sit "
           "between WAIT_DESCRIPTIONS and ACQUIRE_TEXTURES, the two waits the only exits",
           all(p >= 0 for p in positions) and positions == sorted(positions)
           and "break;" not in stage5 and "break;" not in stage7
           and stage6.count("break;") == 1 and stage8.count("break;") == 1
           and "GetCount() < miResourceCount" in stage6 and "GetCount() < miResourceCount" in stage8)
    yield ("stage 5 asks pool 13 for each array's preset mesh collection; stage 6 hands each type-4 reply's handle to "
           "maDebris[event id].AcquireMeshCollection (0x8229CEB4..0x8229D024)",
           "Params()->mpMeshCollectionName" in stage5 and "KI_FX_BUNDLE_POOL" in stage5 and "miResourceCount++" in stage5
           and "mReceiverQueue.Clear()" in stage5
           and "KI_EVENT_ACQUIRE_RESOURCE_RESPONSE" in stage6
           and re.search(r"maDebris\[\s*lpReply->miEventId\s*\]\.AcquireMeshCollection\(", stage6) is not None)
    yield ("stage 7 asks for each collection's texture (GetTextureName, +0x90); stage 8 hands each reply to "
           "maDebris[event id].AcquireTexture (0x8229D028..0x8229D1A4)",
           "GetTextureName()" in stage7 and "KI_FX_BUNDLE_POOL" in stage7 and "mReceiverQueue.Clear()" in stage7
           and "KI_EVENT_ACQUIRE_RESOURCE_RESPONSE" in stage8
           and re.search(r"maDebris\[\s*lpReply->miEventId\s*\]\.AcquireTexture\(", stage8) is not None)
    banner = lifecycle[:lifecycle.find("bool ParticleModule::LoadFXBundle(")]
    yield ("the LoadFXBundle banner lists stages 5..8 in the console's order and no longer says the ladder skips them",
           "stages are NOT visited by this" not in banner and "the asm jumps 4 -> 9" not in banner
           and "10 WAIT_DESCRIPTIONS, 5 ACQUIRE_MESH_COLLECTIONS" in banner.replace("\n//", ""))

    header = code_only(tree.read(ARRAY_H))
    yield ("BrnDebrisArray declares the DWARF's AcquireTexture / AcquireMeshCollection / GetTextureName "
           "(BrnDebrisRenderer.h:144 / :151 / :157) and BrnDebrisArray.cpp bodies GetTextureName",
           "void AcquireTexture(" in header and "void AcquireMeshCollection(" in header
           and "const char* GetTextureName() const;" in header and body(tree.read(ARRAY_CPP), GET_TEXTURE_NAME) != "")

    registration = code_only(tree.read(REGISTRATION_CPP))
    yield ("CgsResourceTypeRegistration registers BrnVFXMeshCollectionResourceType (0x10019), between TextureNameMap and "
           "VFXPropCollection as GameDataModule::RegisterResourceTypes @0x82667EA8 does",
           '#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"' in registration
           and 0 <= registration.find("TypeRegistry::Register(&sTextureNameMap")
           < registration.find("TypeRegistry::Register(&sVFXMeshCollection")
           < registration.find("TypeRegistry::Register(&sVFXPropCollection"))

    bat = BUILD_BAT.read_text(encoding="utf-8", errors="replace") if BUILD_BAT.exists() else ""
    yield ("the parent build mounts SharedClasses\\Graphics\\BrnVFXMeshCollectionResourceType.cpp",
           re.search(r'^\s*echo "%SRC%\\SharedClasses\\Graphics\\BrnVFXMeshCollectionResourceType\.cpp"', bat,
                     flags=re.M) is not None)


def numeric(tree):
    lifecycle = tree.read(LIFECYCLE_CPP)
    try:
        load = definition(lifecycle, LOAD_FX_BUNDLE)
        next_reply = definition(lifecycle, NEXT_REPLY)
    except ValueError as error:
        print("NUMERIC: cannot build -- " + str(error))
        return None
    typedef = re.search(r"^[ \t]*typedef\s+CgsResource::Events::AcquireResourceResponse\s+AcquireResponse\s*;",
                        lifecycle, flags=re.M)
    consts = constants_for(lifecycle, load + next_reply)
    get_name = body(tree.read(ARRAY_CPP), GET_TEXTURE_NAME)
    if not get_name:
        print("NUMERIC: this revision has no BrnDebrisArray::GetTextureName")
    shadow = {relative: tree.read(relative) for relative in SHADOW_HEADERS if tree.read(relative)}
    extra = {
        "fxcrashvfx_fxbundle_consts.inc": "\n".join(consts + ([typedef.group(0).strip()] if typedef else [])
                                                    + [next_reply]) + "\n",
        "fxcrashvfx_fxbundle_array.inc": get_name + "\n",
    }
    return compile_and_run(Path(__file__).with_name("FxCrashVfxFxBundle.cpp"), "fxcrashvfx_fxbundle_body.inc",
                           load + "\n", "FxCrashVfxFxBundle", shadow=shadow, extra_sources=(QUEUE_CPP,),
                           extra_files=extra)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None, help="b5 revision to test (default: the working tree)")
    parser.add_argument("--root", default=None, help="a shadow tree root whose src/ files take precedence")
    args = parser.parse_args()
    tree = RootTree(args.rev, args.root)
    checks = list(wiring(tree))
    result = numeric(tree)
    return report("run_fxcrashvfx_fx_bundle", checks, result, NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
