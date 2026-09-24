"""FX-FLOW (crash parity 2026-09-24, review D on 1e9d360c): the host-only tests NEW-PAYBACK-WIRING
added are gone where the console has none, and the facts that make that safe stay true.

The console embeds both managers by value in GameStateModule and calls them untested:
  R1  GameStateModule::OnModeEnd @0x823767E0: MugshotManager::OnRoundEnd(this+0x500) @0x823767F4,
      PaybackManager::OnRoundEnd(this+0x570) @0x823767FC -- no null test before either call.
  R2  GameStateModule::CopyInputDataToPaybackManager @0x8239AA78: the payback manager (this+0x570)
      is written through with no test.
  R3  PreWorldUpdate @0x823A5328 asserts `lpInput != NULL` (@0x823A53E0, :1130) and does not branch
      on the pre-world input buffer; it takes `LockForRead(lpInput)` @0x823A542C and holds it over
      the copy's call @0x823A572C until `UnlockForRead` @0x823A5D7C. The PC leg keeps that lock around
      the call and drops its null test on the buffer.
The PC holds the three objects by pointer. These facts make the pointers non-null wherever R1..R3
dereference them (the removals are only justified while these hold):
  I1  ConstructTakedownBringUp assigns both managers from `new` and dereferences each at once
      (->Construct(this)).
  I2  GameStateModule::Construct calls ConstructTakedownBringUp and allocates the pre-world stand-in.
  I3  Nothing else writes the three pointers: mpMugshotManager only by that `new`; mpPaybackManager and
      mpPreWorldInputBuffer by their `new` and by GameStateModule::Destruct's reset.
  I4  OnModeEnd's only caller is ModeManager::SendModeStopMessages; CopyInputDataToPaybackManager's
      only caller is the pre-world leg in GameStateModule_gUI_00.cpp.
  I5  GameStateModule::Destruct (the only reset of the two) calls into no ModeManager, so no mode can
      end after the managers are released.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxflow_payback_guards.py [--rev <rev>] [--selftest]
--rev reads every source from that b5 revision (the RED side: the fix commit's parent).
--selftest applies one mutation per check to the working tree's text (in memory) and requires that
check to fail on it.
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
GS = "src/GameSource/GameState/"
TD = GS + "GameStateModule_gTD_00.cpp"
UI = GS + "GameStateModule_gUI_00.cpp"
MODULE = GS + "BrnGameStateModule.cpp"
START = GS + "ModeManager/BrnModeManager_Start.cpp"


class Tree:
    def __init__(self, rev, overlay=None):
        self.rev = rev
        self.overlay = overlay or {}
        self._cache = {}

    def read(self, relative):
        if relative in self.overlay:
            return self.overlay[relative]
        if relative not in self._cache:
            if self.rev is None:
                self._cache[relative] = (REPO / relative).read_text(encoding="utf-8-sig")
            else:
                self._cache[relative] = subprocess.run(
                    ["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"], check=True,
                    capture_output=True, text=True, encoding="utf-8").stdout
        return self._cache[relative]

    def game_sources(self):
        """Every .cpp/.h under src/GameSource, as relative paths."""
        if self.rev is None:
            return sorted(p.relative_to(REPO).as_posix() for p in (REPO / "src/GameSource").rglob("*")
                          if p.suffix in (".cpp", ".h"))
        listing = subprocess.run(["git", "-C", str(REPO), "ls-tree", "-r", "--name-only", self.rev,
                                  "src/GameSource"], check=True, capture_output=True, text=True,
                                 encoding="utf-8").stdout
        return [p for p in listing.splitlines() if p.endswith((".cpp", ".h"))]


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def strings_blanked(text):
    """String literals emptied, so a diag string naming a call is not read as the call."""
    return re.sub(r'"(?:\\.|[^"\\\n])*"', '""', text)


def body(source, signature):
    """The balanced body of the definition that starts at `signature` ('' when absent)."""
    start = source.find(signature)
    if start < 0:
        return ""
    depth = 0
    opened = False
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
            opened = True
        elif token[0] == "}":
            depth -= 1
            if opened and depth == 0:
                return source[start:start + token.end()]
    return ""


def squash(text):
    return re.sub(r"\s+", "", code_only(text))


def checks(tree):
    td = tree.read(TD)
    ui = tree.read(UI)
    module = tree.read(MODULE)

    on_mode_end = squash(body(td, "void GameStateModule::OnModeEnd("))
    mugshot_call = on_mode_end.find("mpMugshotManager->OnRoundEnd(lbResetState);")
    payback_call = on_mode_end.find("mpPaybackManager->OnRoundEnd(lbResetState);")
    clear = on_mode_end.find("ClearTakedownRaceCarData();")
    yield ("R1", "OnModeEnd calls MugshotManager then PaybackManager ::OnRoundEnd untested, before the "
                 "takedown clear (0x823767F4 / 0x823767FC)",
           "if(mpMugshotManager" not in on_mode_end and "if(mpPaybackManager" not in on_mode_end
           and 0 <= mugshot_call < payback_call < clear)

    copy = squash(body(td, "void GameStateModule::CopyInputDataToPaybackManager("))
    yield ("R2", "CopyInputDataToPaybackManager writes the payback manager with no null test (0x8239AA78)",
           bool(copy) and "mpPaybackManager==0" not in copy and "if(mpPaybackManager" not in copy
           and "mpPaybackManager->SetTimerInterface(" in copy)

    ui_code = squash(ui)
    leg = "mpPreWorldInputBuffer->LockForRead();CopyInputDataToPaybackManager(mpPreWorldInputBuffer);" \
          "mpPreWorldInputBuffer->UnlockForRead();"
    at = ui_code.find(leg)
    guarded = at >= 0 and ui_code[:at].endswith("if(mpPreWorldInputBuffer!=0){")
    yield ("R3", "the pre-world leg keeps the console's read lock (0x823A542C..0x823A5D7C) around the "
                 "copy and has no null test on the buffer (the console asserts lpInput, 0x823A53E0)",
           at >= 0 and not guarded)

    construct_td = squash(body(td, "void GameStateModule::ConstructTakedownBringUp("))
    yield ("I1", "ConstructTakedownBringUp news both managers and dereferences each at once",
           "mpMugshotManager=newMugshotManager();}mpMugshotManager->Construct(this);" in construct_td
           and "mpPaybackManager=newPaybackManager();}mpPaybackManager->Construct(this);" in construct_td)

    construct = squash(body(module, "void GameStateModule::Construct("))
    yield ("I2", "GameStateModule::Construct allocates the pre-world stand-in and calls ConstructTakedownBringUp",
           "mpPreWorldInputBuffer=newGameStateModuleIO::PreWorldInputBuffer();" in construct
           and "ConstructTakedownBringUp();" in construct)

    writers = {"mpMugshotManager": [], "mpPaybackManager": [], "mpPreWorldInputBuffer": []}
    callers = {"OnModeEnd": [], "CopyInputDataToPaybackManager": []}
    # A call of GameStateModule's own member: through a GameStateModule pointer / object / accessor,
    # or unqualified inside one of GameStateModule's own TUs (other classes' OnModeEnd are not it).
    receiver = (r"\b(?:mpGameStateModule|lpGameStateModule|mGameStateModule|GetGameStateModule\(\))"
                r"\s*(?:->|\.)\s*")
    for path in tree.game_sources():
        text = strings_blanked(code_only(tree.read(path)))
        for name in writers:
            for match in re.finditer(r"(?<![\w.>])" + name + r"\s*=(?!=)([^;]*);", text):
                writers[name].append((path, re.sub(r"\s+", "", match.group(1))))
        own_tu = re.search(r"/(?:Brn)?GameStateModule[^/]*\.cpp$", path) is not None
        for name in callers:
            for match in re.finditer(receiver + name + r"\s*\(", text):
                callers[name].append(path)
            if not own_tu:
                continue
            for match in re.finditer(r"(?<![\w:~.>])" + name + r"\s*\(", text):
                line_start = text.rfind("\n", 0, match.start()) + 1
                line = text[line_start:text.find("\n", match.start())]
                if re.search(r"\bvoid\b", line):
                    continue   # the definition
                callers[name].append(path)
    destruct = squash(body(module, "void GameStateModule::Destruct("))

    def only(name, allowed):
        seen = writers[name]
        ok = all((path, value) in allowed for path, value in seen) and len(seen) == len(allowed)
        if not ok:
            print("  %s writers: %s" % (name, seen))
        return ok

    header = GS + "BrnGameStateModule.h"
    yield ("I3", "the three pointers have no writer besides their `new`, their header default and "
                 "Destruct's reset",
           only("mpMugshotManager", [(header, "0"), (TD, "newMugshotManager()")])
           and only("mpPaybackManager", [(header, "0"), (TD, "newPaybackManager()"), (MODULE, "0")])
           and only("mpPreWorldInputBuffer", [(header, "0"), (MODULE, "newGameStateModuleIO::PreWorldInputBuffer()"),
                                              (MODULE, "0")])
           and "mpPaybackManager=0;" in destruct and "mpPreWorldInputBuffer=0;" in destruct)

    yield ("I4", "OnModeEnd is called only from SendModeStopMessages, CopyInputDataToPaybackManager only "
                 "from the pre-world leg",
           callers["OnModeEnd"] == [START] and callers["CopyInputDataToPaybackManager"] == [UI]
           and "mpGameStateModule->OnModeEnd(!lbOnlineLobbyHandover);"
               in squash(body(tree.read(START), "void ModeManager::SendModeStopMessages(")))
    if callers["OnModeEnd"] != [START] or callers["CopyInputDataToPaybackManager"] != [UI]:
        print("  callers: %s" % callers)

    yield ("I5", "GameStateModule::Destruct makes no ModeManager call (no mode can end after the reset)",
           bool(destruct) and "mModeManager." not in destruct and "GetModeManager()" not in destruct)


# One mutation per check: (check id, file, old text, new text). Each must turn its check red.
MUTATIONS = [
    ("R1", TD, "    mpMugshotManager->OnRoundEnd(lbResetState);",
     "    if (mpMugshotManager != 0) mpMugshotManager->OnRoundEnd(lbResetState);"),
    ("R2", TD, "    mpPaybackManager->SetTimerInterface(",
     "    if (mpPaybackManager == 0) return;\n    mpPaybackManager->SetTimerInterface("),
    ("R3", UI, "    mpPreWorldInputBuffer->LockForRead();\n    CopyInputDataToPaybackManager(mpPreWorldInputBuffer);",
     "    CopyInputDataToPaybackManager(mpPreWorldInputBuffer);\n    mpPreWorldInputBuffer->LockForRead();"),
    ("I1", TD, "    mpPaybackManager->Construct(this);", "    // (Construct moved)"),
    ("I2", MODULE, "    ConstructTakedownBringUp();", "    // ConstructTakedownBringUp moved"),
    ("I3", TD, "    mpMugshotManager->OnRoundEnd(lbResetState);",
     "    mpMugshotManager->OnRoundEnd(lbResetState);\n    mpMugshotManager = 0;"),
    ("I4", TD, "    mpPaybackManager->SetDirtyTrickButtonState(",
     "    OnModeEnd(false);\n    mpPaybackManager->SetDirtyTrickButtonState("),
    ("I5", MODULE, "        mpPaybackManager->Destruct();",
     "        mModeManager.Destruct();\n        mpPaybackManager->Destruct();"),
]


def run(tree, quiet=False):
    results = list(checks(tree))
    if not quiet:
        for check_id, label, passed in results:
            print(("PASS  " if passed else "FAIL  ") + check_id + "  " + label)
    return {check_id: passed for check_id, _, passed in results}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", help="read the b5 sources from this git revision")
    parser.add_argument("--selftest", action="store_true", help="mutation-test every check")
    args = parser.parse_args()
    tree = Tree(args.rev)
    results = run(tree)
    failed = [check_id for check_id, passed in results.items() if not passed]
    print("run_fxflow_payback_guards: %d/%d pass (%d fail)" % (len(results) - len(failed), len(results), len(failed)))
    if args.selftest:
        bitten = 0
        for check_id, path, old, new in MUTATIONS:
            text = tree.read(path)
            if old not in text:
                print("SELFTEST  %s: mutation anchor missing in %s" % (check_id, path))
                continue
            mutated = run(Tree(args.rev, {path: text.replace(old, new, 1)}), quiet=True)
            if not mutated[check_id]:
                bitten += 1
            else:
                print("SELFTEST  %s: the mutation did NOT turn the check red" % check_id)
        print("selftest: %d/%d mutations caught" % (bitten, len(MUTATIONS)))
        if bitten != len(MUTATIONS):
            return 1
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
