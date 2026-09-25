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
      mpPreWorldInputBuffer by their `new` and by GameStateModule::Destruct's reset. They are PRIVATE
      GameStateModule members, so an unqualified write counts when it sits in GameStateModule's class
      body or in one of its member functions (any file); a write through a GameStateModule receiver
      counts anywhere. (Before 2026-09-25 every same-named write counted, so b5 ca4ac341's
      BurnoutSkillzManager::SetMugshotManager -- its own mpMugshotManager -- read as a writer.)
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


_REV_TEXTS = {}   # (rev, relative) -> text; shared by every Tree (--selftest builds one per mutation)


def _prefetch(rev, paths):
    """Read `paths` of revision `rev` with ONE `git cat-file --batch` (the per-file `git show` of the
    first version took ~45 min for a --rev --selftest). Same text as `git show` in text mode: UTF-8,
    universal newlines."""
    wanted = [p for p in paths if (rev, p) not in _REV_TEXTS]
    if not wanted:
        return
    result = subprocess.run(["git", "-C", str(REPO), "cat-file", "--batch"], check=True, capture_output=True,
                            input="".join(f"{rev}:{p}\n" for p in wanted).encode("utf-8"))
    data, at = result.stdout, 0
    for path in wanted:
        end = data.index(b"\n", at)
        header = data[at:end].split()
        at = end + 1
        if len(header) < 3 or header[1] != b"blob":
            continue   # missing at this revision: read() falls back to `git show` (and its error)
        size = int(header[2])
        _REV_TEXTS[(rev, path)] = data[at:at + size].decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
        at += size + 1


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
            elif (self.rev, relative) in _REV_TEXTS:
                self._cache[relative] = _REV_TEXTS[(self.rev, relative)]
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
        paths = [p for p in listing.splitlines() if p.endswith((".cpp", ".h"))]
        _prefetch(self.rev, paths)
        return paths


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


def blanked(match):
    """A regex match replaced by spaces (newlines kept), so positions survive."""
    return re.sub(r"[^\n]", " ", match.group(0))


CONTROL_HEADER = re.compile(r"(?:else\b|if\b|for\b|while\b|switch\b|do\b|try\b|catch\b|case\b|default\b|return\b"
                            r"|namespace\b|enum\b|extern\b)")
CLASS_HEADER = re.compile(r"(?:template\s*<.*>\s*)?(?:class|struct|union)\b(?P<rest>.*)$")
FUNCTION_NAME = re.compile(r"((?:\w+\s*::\s*)*)(?:~?\w+|operator\s*[^\s(]+)\s*\(")


def enclosing_class(text, position):
    """The class whose member an unqualified name at `position` of `text` (comments, strings and
    preprocessor lines already removed) would be: the class of the innermost enclosing member-function
    definition (`Class::Method(...) {`), or of the innermost enclosing class body. Control-flow blocks,
    lambdas and plain braces are transparent. None at namespace scope or in a free function."""
    stack = []
    for brace in re.finditer(r"[{}]", text[:position]):
        if brace.group(0) == "{":
            stack.append(brace.start())
        elif stack:
            stack.pop()
    for opened in reversed(stack):
        start = max(text.rfind(";", 0, opened), text.rfind("}", 0, opened), text.rfind("{", 0, opened)) + 1
        header = " ".join(text[start:opened].split())
        header = re.sub(r"^(?:(?:public|private|protected)\s*:\s*)+", "", header)
        if not header or CONTROL_HEADER.match(header):
            continue
        if "(" in header and re.search(r"\)\s*(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?::.*)?$", header):
            function = FUNCTION_NAME.search(header)
            if function and function.group(1):
                return re.sub(r"\s+", "", function.group(1)).rstrip(":").split("::")[-1]
            continue   # an in-class inline method or a lambda: the class is further out
        declared = CLASS_HEADER.match(header)
        if declared:
            names = [word for word in re.findall(r"\w+", re.split(r"(?<!:):(?!:)", declared.group("rest"), maxsplit=1)[0])
                     if word != "final"]
            return names[-1] if names else None
    return None


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
        scoped = None
        for name in writers:
            # The three pointers are PRIVATE GameStateModule members (BrnGameStateModule.h), so an
            # unqualified write reaches them only from GameStateModule's own class body or member
            # functions; another class's same-named member (ca4ac341: BurnoutSkillzManager::
            # SetMugshotManager's `mpMugshotManager = lpMugshotManager`) is not a writer of them.
            for match in re.finditer(r"(?<![\w.>])" + name + r"\s*=(?!=)([^;]*);", text):
                if scoped is None:
                    # Same length as `text`: char literals and preprocessor lines (with their
                    # continuations) blanked, so their braces cannot unbalance the scope walk.
                    scoped = re.sub(r"'(?:\\.|[^'\\\n])'", blanked, text)
                    scoped = re.sub(r"(?m)^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", blanked, scoped)
                if enclosing_class(scoped, match.start()) == "GameStateModule":
                    writers[name].append((path, re.sub(r"\s+", "", match.group(1))))
            # A write through a GameStateModule receiver counts wherever it is (a friend could write one).
            for match in re.finditer(receiver + name + r"\s*=(?!=)([^;]*);", text):
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
    # ...a GameStateModule member in another TU (Destruct) resetting the mugshot manager too,
    ("I3", MODULE, "        mpPaybackManager->Destruct();",
     "        mpPaybackManager->Destruct();\n        mpMugshotManager = 0;"),
    # ...and a write through a GameStateModule receiver from another class (ModeManager). The anchor
    # is the code line (a comment above it quotes the same call).
    ("I3", START, "\n    mpGameStateModule->OnModeEnd(",
     "\n    mpGameStateModule->mpPaybackManager = 0;\n    mpGameStateModule->OnModeEnd("),
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
