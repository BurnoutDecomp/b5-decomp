"""FX-RUMBLE (crash parity 2026-09-22, G10-D1/D2/D3/D5/D7): the BrnGameState::RumbleManager chain.

Two halves, both reading the PRODUCTION source (working tree, or a b5 revision with --rev):
  1. WIRING -- structural checks that the GameState module owns and drives the manager exactly
     where the console does (each check cites the ARTIST address that proves it).
  2. NUMERIC -- tests/RumbleManager.cpp compiled against the extracted production bodies
     (Construct / Prepare / Update / UpdateImpacts / OnVehicleAggressorImpact / OnVehicleVictimImpact /
     PlayJolt + the file-scope constants) and the revision's own BrnRumbleManager.h, with the real
     RCEntityActiveRaceCarOutputInterface / ContactSpyInterface / ContactSpyData types.

Run from the workflow checkout (this shell needs NoDefaultCurrentDirectoryInExePath unset):
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_rumble_manager.py [--rev <b5 rev>]
--rev reads every b5 source from that revision (the RED side: --rev <fix commit>~1). A revision whose
BrnRumbleManager.cpp lacks the bodies cannot build the numeric test at all: every numeric check is
reported failed, with the list of absent bodies.
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent

RUMBLE_H = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.h"
RUMBLE_CPP = "src/GameSource/GameState/RumbleManager/BrnRumbleManager.cpp"
GSM_H = "src/GameSource/GameState/BrnGameStateModule.h"
GSM_CPP = "src/GameSource/GameState/BrnGameStateModule.cpp"
GUI_CPP = "src/GameSource/GameState/GameStateModule_gUI_00.cpp"
RCIF_CPP = "src/GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRCEntityActiveRaceCarOutputInterface.cpp"
SPY_CPP = "src/GameSource/Physics/ContactSpies/BrnContactSpyInterface.cpp"

BODIES = [
    "    void RumbleManager::Construct(",
    "    bool RumbleManager::Prepare(",
    "    void RumbleManager::Update(",
    "    void RumbleManager::UpdateImpacts(",
    "    void RumbleManager::OnVehicleAggressorImpact(",
    "    void RumbleManager::OnVehicleVictimImpact(",
    "    void RumbleManager::PlayJolt(",
]
# The number of numeric checks tests/RumbleManager.cpp runs on a complete tree (it prints its own
# count; this is only used to size the failure when a revision cannot build the test at all).
NUMERIC_CHECKS = 86


class Tree:
    def __init__(self, rev, overrides=None):
        self.rev = rev
        self.overrides = overrides or {}

    def read(self, relative):
        if relative in self.overrides:
            return Path(self.overrides[relative]).read_text(encoding="utf-8-sig")
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                              check=True, capture_output=True, text=True, encoding="utf-8").stdout


def definition(source, signature):
    start = source.index(signature)
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return source[start:start + token.end()]
    raise ValueError("unterminated body: " + signature)


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def body_or_empty(source, signature):
    try:
        return code_only(definition(source, signature))
    except ValueError:
        return ""


def settings(name):
    return [line for line in (WORKFLOW / "tools/build" / name).read_text().splitlines()
            if line and not line.startswith("#")]


# ------------------------------------------------------------------------------------------------
# 1. WIRING
# ------------------------------------------------------------------------------------------------
def wiring_checks(tree):
    header = code_only(tree.read(GSM_H))
    gsm = tree.read(GSM_CPP)
    gui = tree.read(GUI_CPP)

    # G10-D5: DWARF BrnGameStateModule.h:775 -- the manager embedded by value (gsm+46680).
    yield ("D5 GameStateModule embeds `RumbleManager mRumbleManager` (DWARF :775)",
           re.search(r"^\s*RumbleManager\s+mRumbleManager\s*;", header, re.M) is not None)
    # G10-D1/D2: DWARF :828 -- the gsm+250800 contact-spy handle RumbleManager::Update reads.
    yield ("D1 GameStateModule holds `ContactSpyInterface mContactSpyInterface` (DWARF :828, gsm+250800)",
           re.search(r"ContactSpy::ContactSpyInterface\s+mContactSpyInterface\s*;", header) is not None)

    construct = body_or_empty(gsm, "void GameStateModule::Construct()")
    drive = construct.find("mDriveThruManager.Construct(")
    rumble = construct.find("mRumbleManager.Construct()")
    yield ("D5 Construct calls mRumbleManager.Construct() (0x823805C8) right after DriveThruManager (#18)",
           rumble >= 0 and drive >= 0 and drive < rumble)
    spy = construct.find("mContactSpyInterface.Construct()")
    carry = construct.find("mGameEventCarryQueue.Construct()")
    yield ("D1 Construct clears the gsm+250800 handle (ContactSpyInterface::Construct 0x8238065C) before the carry queue",
           spy >= 0 and carry >= 0 and spy < carry)

    prepare = body_or_empty(gsm, "bool GameStateModule::Prepare(")
    stage = re.search(r"case\s+E_PREPARESTAGE_RUMBLE_MANAGER\s*:(.*?)case\s+E_PREPARESTAGE_DONE", prepare, re.S)
    yield ("D5 Prepare stage 25 is `if (!mRumbleManager.Prepare()) break;` (0x8239EC24)",
           stage is not None and re.search(r"if\s*\(\s*!\s*mRumbleManager\.Prepare\(\)\s*\)\s*break\s*;", stage.group(1)) is not None)

    pump = body_or_empty(gui, "void GameStateModule::PreWorldUpdateStuntBringUp(")
    update = re.search(r"mRumbleManager\.Update\(\s*&mLastActiveRaceCarInterface\s*,[^;]*?,\s*mePlayerActiveRaceCarIndex\s*,"
                       r"\s*&mContactSpyInterface\s*,\s*lfGameTimestep\s*\)", pump, re.S)
    yield ("D1 PreWorldUpdate calls RumbleManager::Update(&mLastActiveRaceCarInterface, crashQ, mePlayerActiveRaceCarIndex,"
           " &mContactSpyInterface, dt) (0x823A5800)", update is not None)
    drive_update = pump.find("mDriveThruManager.Update(")
    first_arm = pump.find("ProcessGameEventsCarCustomizationBringUp(")
    yield ("D1 ...after DriveThruManager::Update (#41) and before the ProcessGameEvents walk (#68)",
           update is not None and 0 <= drive_update < update.start() and first_arm > update.start())
    pause = re.search(r"mRumbleManager\.UpdatePauseState\(\s*miSimPauseFlags\s*!=\s*0\s*,\s*lpActionQueue\s*\)", pump)
    emm = pump.find("mModeManager.PreWorldUpdate(")
    scoring = pump.find("CopyScoringDataToOutput(")
    yield ("D1 PreWorldUpdate calls UpdatePauseState(miSimPauseFlags != 0, actionQueue) (0x823A5AC4)", pause is not None)
    yield ("D1 ...after EmmPreWorldUpdate (#86) and before CopyScoringDataToOutput (#90)",
           pause is not None and 0 <= emm < pause.start() < scoring)

    post = body_or_empty(gui, "void GameStateModule::PostWorldUpdateStuntBringUp(")
    cache = post.find("CacheTakedownManagerPostWorldInputData(")
    store = re.search(r"mContactSpyInterface\s*=\s*\*\s*lpContactSpyInterface\s*;", post)
    clear = post.find("mContactSpyInterface.Construct()")
    yield ("D1 PostWorldUpdate caches the frame's contact spy into gsm+250800 (0x82375EE0 clear, 0x82375F1C store)",
           store is not None and 0 <= clear < store.start())
    yield ("D1 ...at the CacheTakedownManagerPostWorldInputData point (#18)",
           store is not None and 0 <= cache < clear)

    arm = body_or_empty(gui, "void GameStateModule::ProcessGameEventsVehicleImpactBringUp(")
    send = arm.find("SendVehicleImpactMessages(")
    aggressor = re.search(r"if\s*\(\s*lpImpact->meAggressorActiveRaceCarIndex\s*==\s*GetPlayerActiveRaceCarIndex\(\)\s*\)\s*\{\s*"
                          r"mRumbleManager\.OnVehicleAggressorImpact\(", arm)
    victim = re.search(r"if\s*\(\s*lpImpact->meVictimActiveRaceCarIndex\s*==\s*GetPlayerActiveRaceCarIndex\(\)\s*\)\s*\{\s*"
                       r"mRumbleManager\.OnVehicleVictimImpact\(", arm)
    yield ("D3 case 31: aggressor == player -> OnVehicleAggressorImpact(type) (0x823A27C8)",
           aggressor is not None and 0 <= send < aggressor.start())
    yield ("D3 case 31: victim == player -> OnVehicleVictimImpact(type) (0x823A27EC), after the aggressor leg",
           victim is not None and aggressor is not None and aggressor.start() < victim.start())


# ------------------------------------------------------------------------------------------------
# 2. NUMERIC
# ------------------------------------------------------------------------------------------------
def numeric(tree):
    rumble_cpp = tree.read(RUMBLE_CPP)
    missing = [s.strip() for s in BODIES if s not in rumble_cpp]
    if missing:
        print("NUMERIC: cannot build -- production bodies absent: " + ", ".join(missing))
        return NUMERIC_CHECKS, NUMERIC_CHECKS

    # The TU's own includes come along (the bodies are compiled in their production context).
    includes = re.findall(r"^#include [^\n]+", rumble_cpp, re.M)
    constants = re.findall(r"^    (?:static )?const (?:f32|s32|u32) K[FNU]_[A-Z_]+\s*=[^;]+;", rumble_cpp, re.M)
    methods = includes + ["namespace BrnGameState {"] + constants + [definition(rumble_cpp, s) for s in BODIES] + ["}"]

    rcif = tree.read(RCIF_CPP)
    methods += ["namespace BrnWorld { namespace RaceCarEntityModuleIO {",
                definition(rcif, "const RCEntityActiveRaceCarOutputInterface::RaceCarState*\n"
                                 "RCEntityActiveRaceCarOutputInterface::GetRaceCarState(EActiveRaceCarIndex leActiveRaceCarIndex) const"),
                definition(rcif, "f32 RCEntityActiveRaceCarOutputInterface::TimePlayerInAir() const"),
                "} }"]
    spy = tree.read(SPY_CPP)
    methods += ["namespace BrnPhysics { namespace ContactSpy {",
                definition(spy, "    ContactSpyInterface* ContactSpyInterface::Construct()"),
                definition(spy, "    const ContactSpyData::RaceCarContactRunList* ContactSpyInterface::GetRaceCarContactRunList() const"),
                "} }"]

    with tempfile.TemporaryDirectory(prefix="brn_rumble_") as directory:
        output = Path(directory)
        (output / "rumble_methods.inc").write_text("\n".join(methods), encoding="utf-8")
        # The revision's own header shadows the tree's (first /I wins).
        shadow = output / "shadow" / Path(RUMBLE_H).relative_to("src")
        shadow.parent.mkdir(parents=True)
        shadow.write_text(tree.read(RUMBLE_H), encoding="utf-8")
        includes = f'/I"{output / "shadow"}" /I"{output}" ' + " ".join(
            f'/I"{WORKFLOW / path}"' for path in settings("msvc_includes.txt"))
        command = ("cl " + " ".join(settings("msvc_flags.txt"))
                   + " /D_ALLOW_KEYWORD_MACROS=1 /Dprivate=public /Dprotected=public " + includes
                   + f' "{Path(__file__).with_name("RumbleManager.cpp")}"'
                   + " /Fe:regression.exe /link /OPT:REF")
        script = output / "run.cmd"
        script.write_text('@echo off\ncall "' + str(WORKFLOW / "tools/build/msvc_env.bat")
                          + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                          + "\nif errorlevel 1 exit /b 90\nregression.exe\nexit /b %ERRORLEVEL%\n",
                          encoding="utf-8", newline="\r\n")
        result = subprocess.run(["cmd", "/c", str(script)], cwd=output, capture_output=True, text=True)
        sys.stdout.write(result.stdout[-6000:])
        sys.stderr.write(result.stderr[-6000:])
        match = re.search(r"RumbleManager: (\d+) checks, (\d+) failures", result.stdout)
        if match is None:
            print(f"NUMERIC: the test did not run (exit {result.returncode})")
            return NUMERIC_CHECKS, NUMERIC_CHECKS
        return int(match.group(1)), int(match.group(2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--rumble-cpp", help="read BrnRumbleManager.cpp from this file instead (mutation runs)")
    args = parser.parse_args()
    tree = Tree(args.rev, {RUMBLE_CPP: args.rumble_cpp} if args.rumble_cpp else None)

    wiring = list(wiring_checks(tree))
    for name, passed in wiring:
        print(("PASS  " if passed else "FAIL  ") + name)
    wiring_failures = sum(1 for _, passed in wiring if not passed)

    numeric_checks, numeric_failures = numeric(tree)

    total = len(wiring) + numeric_checks
    failures = wiring_failures + numeric_failures
    print(f"run_rumble_manager: wiring {len(wiring) - wiring_failures}/{len(wiring)}, "
          f"numeric {numeric_checks - numeric_failures}/{numeric_checks}; "
          f"total {total - failures}/{total} pass ({failures} fail)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
