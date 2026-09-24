"""FX-RCEM4 (crash parity 2026-09-24, reviewer A on 65eadffe item 3): the [crash-exit],
[persist-damage] and [intro-timer] witnesses of the race-car module printed on EVERY run, gated only
on gpDebugPrint. They now print only under an env latch, like the other BRN_*_DIAG witnesses:
BRN_CRASH_EXIT_DIAG (CrashExitDiagEnabled, BrnRaceCarEntityModule_CrashExit.cpp) and
BRN_INTRO_TIMER_DIAG (IntroTimerDiagEnabled, BrnRaceCarEntityModule.cpp).

Run from the workflow checkout:
    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxrcem4_diag_latch.py [--pre-fix <b5 rev>]
Structural: every code string literal that starts one of the three tags must sit in a block whose
`if (...)` condition calls the file's latch, and each latch must read its env variable through getenv.
"""
import re
import sys

sys.dont_write_bytecode = True
from fxrcem3_common import RCEM, pre_fix_rev, read

FILES = {
    RCEM + "BrnRaceCarEntityModule_CrashExit.cpp": ("CrashExitDiagEnabled", "BRN_CRASH_EXIT_DIAG",
                                                     ("[crash-exit]", "[persist-damage]")),
    RCEM + "BrnRaceCarEntityModule.cpp": ("IntroTimerDiagEnabled", "BRN_INTRO_TIMER_DIAG", ("[intro-timer]",)),
}
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\n])*"')


def masked(text):
    return TOKEN.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def enclosing_condition(code, position):
    """The condition of the `if (...)` whose block encloses `position` (None when not in an if block)."""
    depth = 0
    for index in range(position - 1, -1, -1):
        char = code[index]
        if char == "}":
            depth += 1
        elif char == "{":
            if depth:
                depth -= 1
                continue
            head = code[:index].rstrip()
            if not head.endswith(")"):
                return None
            level, start = 0, None
            for back in range(len(head) - 1, -1, -1):
                if head[back] == ")":
                    level += 1
                elif head[back] == "(":
                    level -= 1
                    if level == 0:
                        start = back
                        break
            if start is None or not re.search(r"\bif\s*$", head[:start]):
                return None
            return head[start + 1:-1]
    return None


def main():
    rev = pre_fix_rev(sys.argv)
    failures, checked = [], 0
    for rel, (latch, env, tags) in FILES.items():
        source = read(rel, rev)
        code = masked(source)
        definition = re.search(r"\bbool\s+" + latch + r"\s*\(\s*\)\s*\{([^}]*)\}", code)
        if not definition or env not in source[definition.start():definition.end()]:
            failures.append(f"{rel.split('/')[-1]}: no `bool {latch}()` reading getenv(\"{env}\")")
        for literal in TOKEN.finditer(source):
            text = literal.group(0)
            if not text.startswith('"') or not any(text[1:].startswith(tag) for tag in tags):
                continue
            checked += 1
            condition = enclosing_condition(code, literal.start())
            line = source.count("\n", 0, literal.start()) + 1
            if condition is None or latch + "()" not in condition:
                failures.append(f"{rel.split('/')[-1]}:{line} {text[:48]}... is gated by "
                                f"`{(condition or 'nothing').strip()[:60]}`, not {latch}() ({env})")
    for failure in failures:
        print("FAIL:", failure)
    print(f"diag latch: {checked} tagged witness literal(s), {len(failures)} failures")
    sys.exit(1 if (failures or checked == 0) else 0)


if __name__ == "__main__":
    main()
