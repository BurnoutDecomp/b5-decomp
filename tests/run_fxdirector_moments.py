"""FX-DIRECTOR (crash parity 2026-09-24, conductor HIGH PRIORITY): the crash HIGHLIGHT MOMENTS.

  MainDirector::Update @0x82274070 calls MainDirector::UpdateMoments @0x82250268 at 0x82274348, right after
  UpdateCameraBehavioursPostScene; UpdateMoments builds the frame's MomentSharedInfo and calls
  MomentController::UpdateAllMoments @0x82239DE8, which PreUpdates and Updates every allocated moment. The
  moments exist because each selector's Prepare asks MomentController::NewMoment @0x82255850 for them, and
  NewMoment hands the pooled slot to MomentHandle::Prepare @0x821F7298, which Constructs and Prepares it.
  The PC had the call COMMENTED OUT, no body for either function, a GROUP F NewMoment stub that allocated
  nothing (DirectorLinkStubs.cpp), a MomentHandle::Prepare that never Constructed or Prepared the moment,
  and the controller modelled as a console byte span nothing ever constructed. Every crash then reported
  0 valid moments and fell back to the chase camera.

Numeric: tests/FxDirectorMoments.cpp compiles the extracted production MomentController lifecycle /
UpdateAllMoments / MomentHandle::Prepare against the real controller, pool and moment types (recording
FakeMoments in the real pool), and the production MainDirector::UpdateMoments against a stand-in
director carrying the production member names and the revision's own flag-tail enum. A revision without a
body gets a labelled stand-in (and fails).
Wiring: the lifecycle calls, the member layout, the Construct seeds, the factory's home and arms, the
GROUP F stub's retirement and the per-moment vtable one-liners. The call site itself (0x82274348) is
checked only once CALL_SITE_LIVE is True: it stays gated until the two hollow-shell behaviours the
moments pool (BehaviourBystanderCam, BehaviourFixedCam) are real -- its first live run AV'd there.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxdirector_moments.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, compile_and_run, report


_TOKENS = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"|' + r"'(?:\\.|[^'\\\n])*'")


def code_only(text):
    """Comments out, strings kept, in ONE left-to-right pass. (fxgs_common.code_only strips every
    /*...*/ first, so a `Moments/*.cpp` inside a // comment swallows the code up to the next */.)"""
    def keep(match):
        token = match.group(0)
        if token.startswith("//"):
            return ""
        if token.startswith("/*"):
            return "\n" * token.count("\n") or " "
        return token
    return _TOKENS.sub(keep, text)

MAIN_CPP = "src/GameSource/Director/BrnMainDirector.cpp"
MAIN_H = "src/GameSource/Director/BrnMainDirector.h"
CONTROLLER_CPP = "src/GameSource/Director/MomentController/BrnMomentController.cpp"
SPLIT_CPP = "src/GameSource/Director/MomentController/BrnMomentControllerNewMoment.cpp"
STUBS_CPP = "src/GameSource/Director/DirectorLinkStubs.cpp"
MOMENT_H = "src/GameSource/Director/MomentController/BrnMoment.h"
MOMENT_CPP = "src/GameSource/Director/MomentController/BrnMoment.cpp"
MOMENTS = "src/GameSource/Director/MomentController/Moments/"
ABSTRACT_POOL_CPP = REPO / "src/GameSource/Director/Utils/BrnAbstractPool.cpp"
PLAYER_INFO_CPP = REPO / "src/GameSource/Director/Camera/SharedIO/BrnPlayerInfo.cpp"
NUMERIC_CHECKS = 41
# The call site goes live (and this flips to True) only with the two hollow-shell behaviours made real
# and a clean live crash run with the tick on -- the conductor's condition for the unconditional call.
CALL_SITE_LIVE = False

STAND_IN = "/* [stand-in: no body in this revision] */"
CONTROLLER_BODIES = (
    ("void MomentController::Construct()",
     "void MomentController::Construct() { " + STAND_IN + " }"),
    ("bool MomentController::Prepare()",
     "bool MomentController::Prepare() { " + STAND_IN + " return false; }"),
    ("void MomentController::Destruct()",
     "void MomentController::Destruct() { " + STAND_IN + " }"),
    ("void MomentController::UpdateAllMoments(",
     "void MomentController::UpdateAllMoments(Camera::BehaviourManager&, const MomentSharedInfo&) { "
     + STAND_IN + " }"),
    ("Moment* MomentController::MomentHandle::GetMoment() const", None),
    ("bool MomentController::MomentHandle::Release()", None),
    ("bool MomentController::MomentHandle::Prepare(", None),
)
UPDATE_MOMENTS = "void MainDirector::UpdateMoments(const DirectorInputOutput* lpIO, s32 liPlayerCarIndex)"
UPDATE_MOMENTS_STAND_IN = ("void MainDirector::UpdateMoments(const DirectorInputOutput*, s32) { "
                           + STAND_IN + " }")

# The console's GetInstanceType per concrete moment: `li r3, N ; blr` at the vtable's slot 7.
INSTANCE_TYPES = (
    ("MomentHardStop", "BrnMomentHardStop.cpp", "E_MOMENT_HARD_STOP", "0x827E2F38 li r3, 0"),
    ("MomentHitTraffic", "BrnMomentHitTraffic.cpp", "E_MOMENT_HIT_TRAFFIC", "0x82C296C8 li r3, 1"),
    ("MomentTumbling", "BrnMomentTumbling.cpp", "E_MOMENT_TUMBLING", "0x827DF718 li r3, 2"),
    ("MomentTakedownLookback", "BrnMomentTakedownLookback.cpp", "E_MOMENT_TAKEDOWN_LOOKBACK",
     "0x826D7F68 li r3, 3"),
    ("MomentPassengerSeesAction", "BrnMomentPassengerSeesAction.cpp", "E_MOMENT_PASSENGER_SEES_ACTION",
     "0x829DA908 li r3, 4"),
    ("MomentBystanderSeesAction", None, "E_MOMENT_BYSTANDER_SEES_ACTION", "0x826658F0 li r3, 5"),
    ("MomentFailSafe", "BrnMomentFailsafe.cpp", "E_MOMENT_FAILSAFE", "0x824B5A18 li r3, 6"),
    ("MomentPlayerJumping", "BrnMomentPlayerJumping.cpp", "E_MOMENT_PLAYER_JUMPING", "0x821F7628 li r3, 7"),
    ("MomentPlayerStunt", "BrnMomentPlayerStunt.cpp", "E_MOMENT_PLAYER_STUNT", "0x821F7640 li r3, 8"),
    ("MomentStaticCamImpact", "BrnMomentStaticCamImpact.cpp", "E_MOMENT_STATIC_CAM_IMPACT",
     "0x821F7658 li r3, 9"),
    ("MomentNewCarJoined", "BrnMomentNewCarJoined.cpp", "E_MOMENT_NEW_CAR_JOINED", "0x828A80F0 li r3, 0xA"),
    ("MomentStationaryCrash", "BrnMomentStationaryCrash.cpp", "E_MOMENT_STATIONARY_CRASH",
     "0x828A80E8 li r3, 0xB"),
)
# The ten types NewMoment allocates for real (3 TakedownLookback and 7 PlayerJumping are GATED).
ALLOCATED = ("MomentHardStop", "MomentHitTraffic", "MomentTumbling", "MomentPassengerSeesAction",
             "MomentBystanderSeesAction", "MomentFailSafe", "MomentPlayerStunt", "MomentStaticCamImpact",
             "MomentNewCarJoined", "MomentStationaryCrash")


def squash(text):
    return re.sub(r"\s+", "", text)


def find_definition(tree, relatives, signature):
    """(definition text, file) of the first comment-free definition of `signature`, or (None, None)."""
    for relative in relatives:
        source = code_only(tree.read(relative))
        if signature in source:
            try:
                return definition(source, signature), relative
            except ValueError:
                pass
    return None, None


def brace_block(text, start):
    brace = text.find("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise ValueError("unterminated block")


def flag_tail_enum(tree):
    """The revision's EStateFlagTailByte enum text and its {name: value}."""
    header = code_only(tree.read(MAIN_H))
    start = header.find("enum EStateFlagTailByte")
    if start < 0:
        return "", {}
    text = brace_block(header, start)
    values = {name: int(value, 0) for name, value in re.findall(r"(E_FLAG_TAIL_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)", text)}
    return text + ";", values


def wiring(tree):
    main_cpp = code_only(tree.read(MAIN_CPP))

    def body(signature):
        try:
            return squash(definition(main_cpp, signature))
        except ValueError:
            return ""

    update = body("void MainDirector::Update(const DirectorInputOutput* lpIO)")
    if CALL_SITE_LIVE:
        # The console's UNCONDITIONAL call, adjacent to the behaviour pass (no guard between them).
        yield ("MainDirector::Update calls UpdateMoments(lpIO, liPlayerCarIndex) (0x82274348) right after "
               "UpdateCameraBehavioursPostScene, unconditionally, and before UpdateArbitrator",
               "UpdateCameraBehavioursPostScene(lpIO,liPlayerCarIndex);UpdateMoments(lpIO,liPlayerCarIndex);"
               in update and update.find("UpdateMoments(lpIO,liPlayerCarIndex);")
               < update.find("UpdateArbitrator(lpIO,lCamera,liPlayerCarIndex);"))
    else:
        print("PENDING (not counted): the call at 0x82274348 stays gated until Camera::BehaviourBystanderCam "
              "and Camera::BehaviourFixedCam are real Behaviours (see the GATE in MainDirector::Update)")

    construct = body("void MainDirector::Construct(const DirectorResourceManager* lpResourceManager, f32 lfTime)")
    prepare = body("bool MainDirector::Prepare(")
    destruct = body("void MainDirector::Destruct()")
    yield ("the director's lifecycle drives the controller: Construct (0x8225B540..0x8225B554), PREPARE "
           "stage 5, Destruct (0x8224FCC0)",
           "mMomentController.Construct();" in construct
           and "case5:miPrepareStage=5;mMomentController.Prepare();" in prepare
           and "mMomentController.Destruct();" in destruct)

    yield ("Construct seeds the moment gates 0 / 1 / 1 and the fast-top-down force 1 (0x8225B97C / "
           "0x8225B984 / 0x8225B99C / 0x8225B924)",
           all(seed in construct for seed in ("mbAllowJumpMoment=false;", "mbAllowStuntMoment=true;",
                                              "mbAllowHardStopMoment=true;",
                                              "maStateFlagTail[E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN]=1;")))

    shared = body("void MainDirector::BuildArbStateSharedInfo(")
    yield ("ArbStateSharedInfo::mpMomentController is the director's own controller (+0x172D0), not a cast "
           "of a byte span", "&mMomentController)" in shared and "maMomentController" not in shared)

    header = squash(code_only(tree.read(MAIN_H)))
    yield ("MainDirector HOLDS a MomentController (no console byte span, no second model of its pool)",
           "MomentControllermMomentController;" in header and "maMomentController[" not in header
           and "maMomentBucketFreeQueue" not in header)
    yield ("+0x35428..+0x3542F are DWARF mDisplayAspectRatio (f32) + the four moment flags, not an f64",
           "f32mfDisplayAspectRatio;boolmbAllowJumpMoment;boolmbAllowStuntMoment;boolmbAllowHardStopMoment;"
           "boolmbShowAllCameraNames;" in header and "f64mfConstructTime;" not in header)

    _, values = flag_tail_enum(tree)
    yield ("the flag-tail indices UpdateMoments reads: +0x35437 - +0x35430 == 7, +0x3543C - +0x35430 == 0xC",
           values.get("E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN") == 0x07
           and values.get("E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP") == 0x0C)

    stubs = code_only(tree.read(STUBS_CPP))
    yield ("DirectorLinkStubs.cpp no longer defines MomentController::NewMoment (the GROUP F stub that "
           "allocated nothing)", "MomentController::NewMoment(" not in stubs)

    new_moment, home = find_definition(tree, (CONTROLLER_CPP, SPLIT_CPP), "bool MomentController::NewMoment(")
    handle_prepare, handle_home = find_definition(tree, (CONTROLLER_CPP, SPLIT_CPP),
                                                  "bool MomentController::MomentHandle::Prepare(")
    yield ("NewMoment (DWARF :118) and MomentHandle::Prepare (:235) are defined in the MOUNTED "
           "BrnMomentController.cpp", home == CONTROLLER_CPP and handle_home == CONTROLLER_CPP)
    new_moment = squash(new_moment or "")
    yield ("NewMoment allocates the ten ungated types (jpt_822558C4) and Prepares the handle once after the "
           "switch (0x82255B98)",
           all(f"AllocateVoid<{name}>()" in new_moment for name in ALLOCATED)
           and new_moment.count("lrMomentHandleInOut.Prepare(lVoidHandle,*this,lrBehaviourManager);") == 1)

    moment_h = code_only(tree.read(MOMENT_H))
    for name, filename, expected, witness in INSTANCE_TYPES:
        if filename is None:
            match = re.search(r"GetInstanceType\(\)\s*override\s*\{\s*return\s+(E_MOMENT_\w+)", moment_h)
            got = match.group(1) if match else None
        else:
            text, _ = find_definition(tree, (MOMENTS + filename,), f"Moment::EType {name}::GetInstanceType()")
            match = re.search(r"return\s+(E_MOMENT_\w+)", text or "")
            got = match.group(1) if match else None
        yield (f"{name}::GetInstanceType returns {expected} ({witness}) -- the type NewMoment asserts", got == expected)

    missing = [name for name, filename, _, _ in INSTANCE_TYPES if filename is not None
               and find_definition(tree, (MOMENTS + filename,), f"void {name}::Destruct()")[0] is None]
    yield ("every concrete moment's vtable slot 5 (Destruct, the shared `blr` 0x8284CB38) has a body"
           + (f" -- missing {missing}" if missing else ""), not missing)
    moment_cpp = code_only(tree.read(MOMENT_CPP))
    yield ("Moment's own SetParameters / Destruct (DWARF BrnMoment.cpp:43 / :51) have bodies",
           "void Moment::SetParameters(" in moment_cpp and "void Moment::Destruct()" in moment_cpp)


def numeric(tree):
    parts, missing = [], []
    for signature, stand_in in CONTROLLER_BODIES:
        text, _ = find_definition(tree, (CONTROLLER_CPP, SPLIT_CPP), signature)
        if text is None:
            if stand_in is None:
                print(f"NUMERIC: no body for the required {signature} in this revision")
                return None
            missing.append(signature)
            text = stand_in
        parts.append(text)
    text, _ = find_definition(tree, (MAIN_CPP,), UPDATE_MOMENTS)
    if text is None:
        missing.append(UPDATE_MOMENTS)
        text = UPDATE_MOMENTS_STAND_IN
    parts.append(text)
    for signature in missing:
        print(f"NUMERIC: {signature} has no body in this revision (labelled stand-in)")

    enum_text, values = flag_tail_enum(tree)
    extra = []
    for index, name in enumerate(("E_FLAG_TAIL_FORCE_NEXT_WORLD_CRASH_FAST_TOP_DOWN", "E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP")):
        if name not in values:
            print(f"NUMERIC: {name} is absent from this revision's flag tail (stand-in index)")
            extra.append(f"{name} = {0x1E + index} /* [stand-in: absent in this revision] */")
    flags = ("namespace BrnDirector\n{\nstruct MainDirectorFlagTail\n{\n" + enum_text + "\n"
             + ("enum EStandInFlagTailByte { " + ", ".join(extra) + " };\n" if extra else "")
             + "};\n}\n")
    inc = "namespace BrnDirector\n{\n" + "\n\n".join(parts) + "\n}\n"
    return compile_and_run(Path(__file__).with_name("FxDirectorMoments.cpp"), "director_moments.inc", inc,
                           "FxDirectorMoments", extra_sources=(STRSTREAM_CPP, ABSTRACT_POOL_CPP, PLAYER_INFO_CPP),
                           extra_files={"director_moments_flags.inc": flags})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    return report("run_fxdirector_moments", list(wiring(tree)), numeric(tree), NUMERIC_CHECKS)


if __name__ == "__main__":
    sys.exit(main())
