"""FX-SCENARIOS (crash parity 2026-09-25): the game modes' intro lengths, GameMode vtable slot 8.

  The director's crash mode took a Showtime's frame 6.0 s after the start, with the car already at rest
  (FxDirectorShowtime's rig-travel FAIL(c)). The 6.0 s was not the console's: CrashMode had no slot-8
  override (the console's 0x827E2600 returns flt_82008718 = 0.0002f), so it inherited
  GameMode::GetIntroDurationSeconds -- itself a 6.0f STUB where the console's @0x82315A88 returns 6.0 offline,
  2.0 for mode 15 / 16 and (flyby cars + 1) * 4.0 for the other online modes. IntroState::OnEnter @0x823163C8
  seeds the countdown from slot 8 and times it for mode type 2, so every Showtime sat 6.0 s in E_GMS_INTRO.
  PursuitMode (0x827E24E8, 1.0f) and FaceOffMode (0x827EAB30, 0.0f: no intro) lacked theirs too, and
  CrashMode had no slot-6 GetName (0x827E24C8, "CrashMode").

Sections (SECTIONS below; each fix's commit adds its own):
  crash   -- CrashMode slot 8 + slot 6, through the production IntroState::OnEnter / Update
  base    -- GameMode::GetIntroDurationSeconds @0x82315A88 over the mode types
  pursuit -- PursuitMode slot 8
  faceoff -- FaceOffMode slot 8 and StartModeIntro's `> 0.0f` rule
Wiring checks are structural (comment-stripped); numeric checks compile tests/FxScenariosModeIntro.cpp against
the PRODUCTION bodies of the revision under test (working tree, or --rev <b5 rev>), with the real mode headers
of that revision shadowed in for the declaration checks.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxscenarios_mode_intro.py [--rev <b5 rev>]
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, code_only, compile_and_run, report, STRSTREAM_CPP

SECTIONS = ["crash"]
NUMERIC_PER_SECTION = {"crash": 6, "base": 12, "pursuit": 3, "faceoff": 3}

MODES = "src/GameSource/GameState/ModeManager/GameModes/"
GAME_MODE_CPP = MODES + "BrnGameMode.cpp"
CRASH_H, CRASH_CPP = MODES + "BrnCrashMode.h", MODES + "BrnCrashMode.cpp"
PURSUIT_H, PURSUIT_CPP = MODES + "BrnPursuitMode.h", MODES + "BrnPursuitMode.cpp"
FACEOFF_H, FACEOFF_CPP = MODES + "BrnFaceOffMode.h", MODES + "BrnFaceOffMode.cpp"
INTRO_CPP = "src/GameSource/GameState/ModeManager/GameModeStates/BrnIntroState.cpp"
INTRO_PLAY_CPP = "src/GameSource/GameState/ModeManager/BrnModeManager_IntroPlay.cpp"

BASE_SIG = "f32 GameMode::GetIntroDurationSeconds() const"
CRASH_INTRO_SIG = "f32 CrashMode::GetIntroDurationSeconds() const"
CRASH_NAME_SIG = "const char* CrashMode::GetName() const"
PURSUIT_INTRO_SIG = "f32 PursuitMode::GetIntroDurationSeconds() const"
FACEOFF_INTRO_SIG = "f32 FaceOffMode::GetIntroDurationSeconds() const"
INTRO_BODIES = ["void IntroState::OnEnter()", "void IntroState::Update()"]


def constant_line(source, name):
    match = re.search(r"^[ \t]*(?:static[ \t]+)?const[ \t]+f32[ \t]+" + re.escape(name) + r"[ \t]*=[^;]*;", source, re.M)
    return match.group(0).strip() if match else None


def optional(source, signature):
    try:
        return definition(source, signature)
    except ValueError:
        return None


def class_body(source, name):
    """The comment-stripped body of `class <name> ... { ... };` (empty if absent)."""
    match = re.search(r"\bclass[ \t]+" + re.escape(name) + r"\b[^;{]*\{", source)
    if match is None:
        return ""
    try:
        return code_only(definition(source, source[match.start():match.end()]))
    except ValueError:
        return ""


def declares(source, class_name, pattern):
    return re.search(pattern, class_body(source, class_name)) is not None


def wiring(tree):
    checks = []
    if "crash" in SECTIONS:
        crash_h = tree.read(CRASH_H)
        checks.append(("crash: class CrashMode declares slot 8 `virtual f32 GetIntroDurationSeconds() const` (vtable 0x820D0570 +0x20 = 0x827E2600)",
                       declares(crash_h, "CrashMode", r"virtual\s+f32\s+GetIntroDurationSeconds\s*\(\s*\)\s*const\s*;")))
        checks.append(("crash: class CrashMode declares slot 6 `virtual const char* GetName() const` (vtable 0x820D0570 +0x18 = 0x827E24C8)",
                       declares(crash_h, "CrashMode", r"virtual\s+const\s+char\s*\*\s*GetName\s*\(\s*\)\s*const\s*;")))
    if "base" in SECTIONS:
        base = code_only(optional(tree.read(GAME_MODE_CPP), BASE_SIG) or "")
        checks.append(("base: GameMode::GetIntroDurationSeconds asks the flyby count (ModeManager::GetNumberOfCarsInFlyby, 0x82315AE4)",
                       "GetNumberOfCarsInFlyby()" in base))
        checks.append(("base: ... and tests the current mode's online byte and type 15 / 16 (0x82315AA4 / 0x82315AC0)",
                       "IsOnline()" in base and "E_MODE_ONLINE_FREE_BURN_LOBBY" in base and "E_MODE_ONLINE_SHOWTIME" in base))
    if "pursuit" in SECTIONS:
        checks.append(("pursuit: class PursuitMode declares slot 8 (vtable 0x820D0650 +0x20 = 0x827E24E8)",
                       declares(tree.read(PURSUIT_H), "PursuitMode", r"virtual\s+f32\s+GetIntroDurationSeconds\s*\(\s*\)\s*const\s*;")))
    if "faceoff" in SECTIONS:
        checks.append(("faceoff: class FaceOffMode declares slot 8 (vtable 0x820D0500 +0x20 = 0x827EAB30)",
                       declares(tree.read(FACEOFF_H), "FaceOffMode", r"virtual\s+f32\s+GetIntroDurationSeconds\s*\(\s*\)\s*const\s*;")))
        play = code_only(tree.read(INTRO_PLAY_CPP))
        checks.append(("faceoff: StartModeIntro still sets mbDoIntro = (mfDurationSeconds > 0.0f) (the rule a 0.0 intro meets)",
                       re.search(r"mbDoIntro\s*=\s*\(\s*lIntroAction\.mfDurationSeconds\s*>\s*0\.0f\s*\)", play) is not None))
    return checks


def numeric(tree):
    game_mode = tree.read(GAME_MODE_CPP)
    crash_cpp, pursuit_h, pursuit_cpp = tree.read(CRASH_CPP), tree.read(PURSUIT_H), tree.read(PURSUIT_CPP)
    faceoff_cpp, intro_cpp = tree.read(FACEOFF_CPP), tree.read(INTRO_CPP)
    try:
        parts = ["namespace {"]
        for name in ("KF_INSTANT_INTRO_SECONDS",):
            line = constant_line(intro_cpp, name)
            if line is None:
                raise ValueError("constant absent: " + name)
            parts.append(line)
        for source, name in ((crash_cpp, "KF_SHOWTIME_INTRO_DURATION_SECONDS"), (pursuit_h, "KF_PURSUIT_INTRO_TIME")):
            line = constant_line(source, name)
            if line is not None:
                parts.append(line)
        parts.append("}")
        parts.append(definition(game_mode, BASE_SIG))
        parts += [definition(intro_cpp, body) for body in INTRO_BODIES]
    except ValueError as error:
        print("NUMERIC: cannot build -- production body absent: " + str(error))
        return None
    config = []
    for flag, source, signature in (("FXSC_HAS_CRASH_INTRO", crash_cpp, CRASH_INTRO_SIG),
                                    ("FXSC_HAS_CRASH_NAME", crash_cpp, CRASH_NAME_SIG),
                                    ("FXSC_HAS_PURSUIT_INTRO", pursuit_cpp, PURSUIT_INTRO_SIG),
                                    ("FXSC_HAS_FACEOFF_INTRO", faceoff_cpp, FACEOFF_INTRO_SIG)):
        text = optional(source, signature)
        config.append("#define %s %d" % (flag, 1 if text else 0))
        if text:
            parts.append(text)
    for section in ("crash", "base", "pursuit", "faceoff"):
        config.append("#define FXSC_SECTION_%s %d" % (section.upper(), 1 if section in SECTIONS else 0))
    shadow = {}
    if tree.rev is not None:
        for header in (CRASH_H, PURSUIT_H, FACEOFF_H):
            text = tree.read(header)
            if text:
                shadow[header] = text
    return compile_and_run(Path(__file__).with_name("FxScenariosModeIntro.cpp"), "fxscenarios_mode_intro.inc",
                           "\n".join(parts), "FxScenariosModeIntro", shadow=shadow, extra_sources=[STRSTREAM_CPP],
                           extra_files={"fxscenarios_mode_intro_config.inc": "\n".join(config) + "\n"})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    args = parser.parse_args()
    tree = Tree(args.rev)
    total = sum(NUMERIC_PER_SECTION[section] for section in SECTIONS)
    return report("run_fxscenarios_mode_intro", wiring(tree), numeric(tree), total)


if __name__ == "__main__":
    sys.exit(main())
