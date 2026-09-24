"""FX-NANPOL (crash parity 2026-09-24): the three host-only null [GUARD]s dce59f43 added in World/AI
are gone, and the facts that make them dead stay true.

Independent review A (dce59f43) flagged three invented null tests, each where the console
dereferences unconditionally:
  R1  RaceBalancingManager::Update (the race clock, inlined at 0x8279B678..0x8279B6C0):
      `lbz 0x1542(r3)` @0x8279B698 on GetAICar(mePlayerGlobalRaceCarIndex) -- no null test.
  R2  AIModule::UpdateCarRoutes event-117 gate: 0x8279577C GetAIDriver(active) ; 0x82795780
      `lwz 0x1CE0(r3)` -- no null test on the driver.
  R3  ... 0x8279579C GetAICar(global) ; 0x827957A0 `lwz 0x1408(r3)` (HasValidRoute inlined) -- none.
  R4  AIModule::Update row 12 (0x8279B66C..0x8279B674): GetAICar is called unconditionally; the
      host's `index == INVALID ? 0` mapping only ever fed R1.
The console's GetAICar @0x82765AD0 / GetAIDriver @0x82765B90 assert on a bad index and still return
base + i*size -- never null. The PC's return null only past the array, and these checks pin why
that cannot happen at R1..R3 (the removal is justified only while they hold):
  I1  AIDriver::mbIsActive is raised (= 1/true) ONLY inside AIDriver::SetAICar, which binds
      mpCarHost in the same body;
  I2  every call of SetAICar is preceded by a `lpCar == 0 -> break` test on the car it passes;
  I3  every place that clears a driver's car (SetCar(0) / mpCarHost = 0) also clears mbIsActive;
  I4  AICar::miRaceCarIndex is written only by AICar::Construct, and AIModule::Construct calls it as
      maAICars[leCar].Construct(leCar) -- so a bound car's index is its own slot;
  I5  AIModule::mePlayerGlobalRaceCarIndex is written only by AIModule::Construct (slot 0) and by
      row 11 under the active-driver test;
  I6  UpdateCarRoutes is called once, from AIModule::Update, inside the mbPlayerDataSet gate and
      after mePlayerActiveRaceCarIndex is read from the interface; RaceCarEntityModule::
      WriteUpdatedAIData returns on INVALID before SetPlayerActiveRaceCarData raises the gate.

    env -u NoDefaultCurrentDirectoryInExePath python b5-decomp/tests/run_fxnanpol_guards.py [--rev <rev>]
--rev reads every source from that b5 revision (the RED side: the fix commit's parent).
"""
from pathlib import Path
import argparse
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
AI = "src/GameSource/World/AI/"
RBM = AI + "RaceBalancing/BrnRaceBalancingManager.cpp"
ROUTES = AI + "BrnAIModule_Routes.cpp"
PUMP = AI + "BrnAIModule_ResetPump.cpp"
EVENTS = AI + "BrnAIModule_Events.cpp"
MODULE = AI + "BrnAIModule.cpp"
DRIVER = AI + "BrnAIDriver.cpp"
CAR_UPDATE = AI + "BrnAICar_Update.cpp"
RCEM_PUMP = "src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule_ResetPump.cpp"


class Tree:
    def __init__(self, rev):
        self.rev = rev

    def read(self, relative):
        if self.rev is None:
            return (REPO / relative).read_text(encoding="utf-8-sig")
        return subprocess.run(["git", "-C", str(REPO), "show", f"{self.rev}:{relative}"],
                              check=True, capture_output=True, text=True, encoding="utf-8").stdout

    def ai_sources(self):
        """Every .cpp/.h under World/AI, as {path: text}."""
        if self.rev is None:
            paths = [p.relative_to(REPO).as_posix() for p in (REPO / AI).rglob("*")
                     if p.suffix in (".cpp", ".h")]
        else:
            listing = subprocess.run(["git", "-C", str(REPO), "ls-tree", "-r", "--name-only", self.rev, AI],
                                     check=True, capture_output=True, text=True, encoding="utf-8").stdout
            paths = [p for p in listing.splitlines() if p.endswith((".cpp", ".h"))]
        return {p: self.read(p) for p in paths}


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def body_at(source, start):
    """The balanced {...} block that opens at or after `start` (comments and strings skipped)."""
    depth = 0
    first = None
    for token in re.finditer(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[{}]', source[start:]):
        if token[0] == "{":
            depth += 1
            first = start + token.start() if first is None else first
        elif token[0] == "}":
            depth -= 1
            if depth == 0:
                return first, start + token.end()
    raise ValueError("unterminated block")


def function_body(source, signature):
    start = source.index(signature)
    return source[start:body_at(source, start)[1]]


def enclosing_function(source, index):
    """Name of the innermost `Class::Method(` definition whose body contains `index`."""
    best = None
    for match in re.finditer(r"^[ \t]*[\w:<>\*& \t]*?\b(\w+::~?\w+)\s*\([^;{]*\)\s*(?:const\s*)?\{", source, re.M):
        try:
            open_at, close_at = body_at(source, match.start())
        except ValueError:
            continue
        if open_at <= index < close_at:
            best = match.group(1)
    return best


def checks(tree):
    # ---- R1..R4: the guards are gone -----------------------------------------------------------
    update = code_only(function_body(tree.read(RBM), "void RaceBalancingManager::Update("))
    yield ("R1 race clock reads lpPlayerCar->IsCrashing() with no host null test (0x8279B698)",
           "lpPlayerCar->IsCrashing()" in update
           and not re.search(r"lpPlayerCar\s*(?:!=|==)\s*(?:0|NULL|nullptr)|\(\s*lpPlayerCar\s*&&", update))

    routes = code_only(function_body(tree.read(ROUTES), "void AIModule::UpdateCarRoutes("))
    yield ("R2 UpdateCarRoutes: GetAIDriver(active)->GetCar() with no driver null test (0x82795780)",
           re.search(r"lpPlayerDriver->GetCar\(\)\s*!=\s*0", routes) is not None
           and not re.search(r"lpPlayerDriver\s*(?:!=|==)\s*(?:0|NULL|nullptr)", routes))
    yield ("R3 UpdateCarRoutes: GetAICar(global)->HasValidRoute() with no car null test (0x827957A0)",
           "lpPlayerAICar->HasValidRoute()" in routes
           and not re.search(r"lpPlayerAICar\s*(?:!=|==)\s*(?:0|NULL|nullptr)", routes))

    pump = tree.read(PUMP)
    ai_update = code_only(function_body(pump, "void AIModule::Update("))
    yield ("R4 AIModule::Update row 12 calls GetAICar(mePlayerGlobalRaceCarIndex) unconditionally (0x8279B674)",
           re.search(r"AICar\*\s*lpPlayerCar\s*=\s*GetAICar\(\s*static_cast<u32>\(\s*mePlayerGlobalRaceCarIndex\s*\)\s*\)\s*;",
                     ai_update) is not None)

    # ---- I1..I6: why the pointers cannot be null there ----------------------------------------
    sources = tree.ai_sources()
    raises = [(p, m.start()) for p, s in sources.items() for m in re.finditer(r"\bmbIsActive\s*=\s*(?:1|true)\b", code_only(s))]
    raise_owners = {enclosing_function(code_only(sources[p]), i) for p, i in raises}
    driver = tree.read(DRIVER)
    set_ai_car = code_only(function_body(driver, "void AIDriver::SetAICar("))
    yield ("I1 mbIsActive is raised only in AIDriver::SetAICar, which binds mpCarHost in the same body",
           len(raises) >= 1 and raise_owners == {"AIDriver::SetAICar"}
           and re.search(r"mpCarHost\s*=\s*lpCar\s*;", set_ai_car) is not None)

    calls_ok = True
    call_count = 0
    for p, s in sources.items():
        code = code_only(s)
        for m in re.finditer(r"(\w+)\s*->\s*SetAICar\(\s*(\w+)\s*\)", code):
            call_count += 1
            head = code[max(0, m.start() - 1500):m.start()]
            arg = m.group(2)
            if not re.search(r"if\s*\(\s*" + re.escape(arg) + r"\s*==\s*0\s*\)\s*\{\s*break\s*;", head):
                calls_ok = False
    yield ("I2 every SetAICar call is preceded by a `car == 0 -> break` test", call_count >= 1 and calls_ok)

    clears_ok = True
    for p, s in sources.items():
        code = code_only(s)
        for m in re.finditer(r"SetCar\(\s*(?:0|NULL|nullptr)\s*\)|\bmpCarHost\s*=\s*(?:0|NULL|nullptr)\b", code):
            owner = enclosing_function(code, m.start())
            if owner is None:
                clears_ok = False
                continue
            # the enclosing function's text: from its definition to its closing brace
            body = None
            for fm in re.finditer(r"^[ \t]*[\w:<>\*& \t]*?\b" + re.escape(owner) + r"\s*\(", code, re.M):
                open_at, close_at = body_at(code, fm.start())
                if open_at <= m.start() < close_at:
                    body = code[open_at:close_at]
                    break
            if body is None or not re.search(r"\bmbIsActive\s*=\s*(?:0|false)\b", body):
                clears_ok = False
    yield ("I3 every driver-car clear sits in a body that also clears mbIsActive", clears_ok)

    car_writes = [(p, m.start()) for p, s in sources.items() for m in re.finditer(r"\bmiRaceCarIndex\s*=(?!=)", code_only(s))]
    car_owners = {enclosing_function(code_only(sources[p]), i) for p, i in car_writes}
    module = code_only(tree.read(MODULE))
    yield ("I4 AICar::miRaceCarIndex is written only by AICar::Construct, called as maAICars[leCar].Construct(leCar)",
           car_owners == {"AICar::Construct"}
           and re.search(r"maAICars\[\s*(\w+)\s*\]\.Construct\(\s*\1\s*\)", module) is not None)

    global_writes = []
    for p, s in sources.items():
        if not Path(p).name.startswith("BrnAIModule"):
            continue
        code = code_only(s)
        for m in re.finditer(r"\bmePlayerGlobalRaceCarIndex\s*=(?!=)", code):
            global_writes.append((enclosing_function(code, m.start()), code[m.start():code.find(";", m.start())]))
    owners = sorted({w[0] for w in global_writes})
    row11 = re.search(r"if\s*\(\s*lpPlayerDriver\s*!=\s*0\s*&&\s*lpPlayerDriver->IsActive\(\)\s*\)\s*\{\s*"
                      r"mePlayerGlobalRaceCarIndex\s*=\s*\(\s*lpPlayerDriver->GetCar\(\)\s*!=\s*0\s*\)", ai_update)
    yield ("I5 mePlayerGlobalRaceCarIndex is written only by AIModule::Construct (slot 0) and row 11 under IsActive()",
           owners == ["AIModule::Construct", "AIModule::Update"] and row11 is not None
           and any("E_GLOBAL_RACE_CAR_INDEX_0" in w[1] for w in global_writes if w[0] == "AIModule::Construct"))

    gate = re.search(r"if\s*\(\s*lpRaceCarAI\s*!=\s*0\s*&&\s*lpRaceCarAI->mbPlayerDataSet\s*\)", ai_update)
    ok6 = False
    if gate:
        open_at, close_at = body_at(ai_update, gate.start())
        calls = [m.start() for m in re.finditer(r"\bUpdateCarRoutes\(", ai_update)]
        read = ai_update.find("mePlayerActiveRaceCarIndex = lpRaceCarAI->GetPlayerActiveRaceCarIndex()")
        others = sum(len(re.findall(r"\bUpdateCarRoutes\(\s*lp", code_only(s)))
                     for p, s in sources.items() if p != PUMP)
        rcem = code_only(function_body(tree.read(RCEM_PUMP), "void RaceCarEntityModule::WriteUpdatedAIData("))
        bail = re.search(r"if\s*\(\s*mePlayerActiveRaceCarIndex\s*==\s*E_ACTIVE_RACE_CAR_INDEX_INVALID\s*\)\s*\{\s*return\s*;", rcem)
        publish = rcem.find("SetPlayerActiveRaceCarData(")
        ok6 = (len(calls) == 1 and open_at < read < calls[0] < close_at and others == 0
               and bail is not None and 0 <= bail.start() < publish)
    yield ("I6 UpdateCarRoutes runs once, inside the mbPlayerDataSet gate, after the published active index", ok6)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rev", default=None)
    tree = Tree(parser.parse_args().rev)
    failures = 0
    total = 0
    for label, ok in checks(tree):
        total += 1
        if not ok:
            failures += 1
            print("FAIL:", label)
    print(f"FxNanpolGuards: {total} checks, {failures} failures")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
