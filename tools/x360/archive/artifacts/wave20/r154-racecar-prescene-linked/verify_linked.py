"""Verify the R154 RaceCar PreScene and replay event 592 chain in a sealed PPC image."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


class Image:
    def __init__(self, pe, map_file):
        self.data = pe.read_bytes()
        nt, = struct.unpack_from("<I", self.data, 0x3C)
        assert self.data[nt:nt + 4] == b"PE\0\0", "PE signature"
        machine, count, _, _, _, optional, _ = struct.unpack_from("<HHIIIHH", self.data, nt + 4)
        assert machine == 0x01F2, f"PPC machine expected, got {machine:#x}"
        self.base, = struct.unpack_from("<I", self.data, nt + 52)
        self.sections = [struct.unpack_from("<8sIIII", self.data, nt + 24 + optional + 40 * i)
                         for i in range(count)]
        self.labels = {}
        self.symbols = {}
        for line in map_file.read_text().splitlines():
            match = re.match(r"\s+\w+:\w+\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\b", line)
            if match:
                name, address = match[1], int(match[2], 16)
                self.labels.setdefault(address, []).append(name)
                self.symbols[name] = address

    def address(self, prefix):
        matches = {address for name, address in self.symbols.items() if name.startswith(prefix)}
        assert len(matches) == 1, (prefix, sorted(matches))
        return matches.pop()

    def word(self, address):
        rva = address - self.base
        _, _, start, _, file_at = next(section for section in self.sections
                                      if section[2] <= rva < section[2] + section[3])
        return struct.unpack_from(">I", self.data, file_at + rva - start)[0]

    def target(self, pc, opcode):
        if opcode >> 26 == 18:
            return (0 if opcode & 2 else pc) + signed(opcode & 0x03FFFFFC, 26)
        if opcode >> 26 == 16:
            return (0 if opcode & 2 else pc) + signed(opcode & 0xFFFC, 16)
        return None

    def body(self, entry):
        end = min(address for address in self.labels if address > entry)
        return [(pc, self.word(pc)) for pc in range(entry, end, 4)]

    def calls(self, entry):
        return [(pc, self.target(pc, word)) for pc, word in self.body(entry)
                if word >> 26 == 18 and word & 1]

    def calls_named(self, entry, prefix):
        return [(pc, target) for pc, target in self.calls(entry)
                if any(name.startswith(prefix) for name in self.labels.get(target, []))]

    def call(self, entry, prefix):
        hits = self.calls_named(entry, prefix)
        assert len(hits) == 1, (hex(entry), prefix, hits)
        return hits[0][0]

    def has_call(self, pc, prefix):
        word = self.word(pc)
        return (word >> 26 == 18 and word & 1 and
                any(name.startswith(prefix) for name in self.labels.get(self.target(pc, word), [])))

    def pattern(self, entry, words):
        end = min(address for address in self.labels if address > entry)
        return [pc for pc in range(entry, end - 4 * len(words) + 4, 4)
                if all(self.word(pc + i * 4) & mask == word for i, (word, mask) in enumerate(words))]


PRE = "?PreSceneUpdate@RaceCarEntityModule@"
WORLD = "?Update@WorldModule@"
BRIDGE = "?BridgeRaceCarEntityInfoToOutput_PreScene@WorldModule@"
GUI_BRIDGE = "?BridgeWorldTrafficAndPropDataToGui@BrnGameModule@"
GUI_CACHE = "?RecEvent@GuiCache@BrnGui@"
GUI_UPDATE = "?Update@GuiModule@BrnGui@"
RENDER = "?GenerateDispatchLists@RaceCarEntityModule@"
SET_FLAG = "?SetReplayRaceCarRendered@OutputBuffer_PreScene@"
GET_FLAG = "?IsReplayRaceCarRendered@OutputBuffer_PreScene@"
GET_TYPE = "?GetActivePaybackType@InputBuffer_PreScene@"
GET_AGGRESSOR = "?GetActivePaybackAggressor@InputBuffer_PreScene@"
GET_CONTROLS = "?GetPlayerVehicleControls@InputBuffer_PreScene@"
CAR_PASS = "?UpdateRaceCars_PreScene@RaceCarEntityModule@"
STREAM = "?UpdateStreaming@RaceCarEntityModule@"
PLAYING = "?IsPlaying@BaseSerialiser@"
AI = "?WriteUpdatedAIData@RaceCarEntityModule@"
GUI_FLAG = "?IsReplayARCRendered@GuiCache@"
GUI_COUNT = "?IncrementReplayPlayerActive@GuiCache@"
READ = "?LockForRead@IOBuffer@"
WRITE = "?LockForWrite@IOBuffer@"
UNREAD = "?UnlockForRead@IOBuffer@"
UNWRITE = "?UnlockForWrite@IOBuffer@"
RW2 = "?LockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@1@Z"
UNRW2 = "?UnlockBuffersForIO@CgsModule@@YAXPAUIOBuffer@1@PBU21@1@Z"
ADD_GUI = "?AddEvent@?$VariableEventQueue@$0IAAA@$0BA@@CgsModule@"
ALL = 0xFFFFFFFF


def landmark(image, pc, expected):
    actual = image.word(pc)
    assert actual == expected, f"{pc:#010x}: {actual:#010x} expected {expected:#010x}"


def controls(image):
    entry = image.address(PRE)
    payback = image.call(entry, GET_TYPE)
    aggressor = image.call(entry, GET_AGGRESSOR)
    getter = image.call(entry, GET_CONTROLS)
    car = image.call(entry, CAR_PASS)
    assert (aggressor - payback, getter - aggressor, car - getter) == (0x18, 0x18, 0x20), (
        "controls must precede car pass", [hex(pc) for pc in (payback, aggressor, getter, car)])
    assert car < image.call(entry, STREAM)
    base = payback - 0x148
    for off, word in {0x13C:0x2B1E0000, 0x140:0x409A005C, 0x154:0x614A83E4,
                      0x15C:0x7D76512E, 0x16C:0x614A83E8, 0x174:0x7D76512E,
                      0x17C:0x7C641B78, 0x180:0x3C760002, 0x184:0x38A0003C,
                      0x188:0x386383A8}.items():
        landmark(image, base + off, word)
    assert image.has_call(base + 0x18C, "memcpy")
    assert image.target(base + 0x140, image.word(base + 0x140)) == car + 4
    return [payback, aggressor, getter, base + 0x18C, car]


def replay_drain(image):
    entry = image.address(PRE)
    gate = image.call(entry, PLAYING)
    setter = image.call(entry, SET_FLAG)
    ai = image.call(entry, AI)
    assert image.call(entry, STREAM) < gate < setter < ai
    for delta, word in { -8:0x3C760002, -4:0x38638600, 0x0C:0x419A006C,
                         0x24:0x3BBD86C1, 0x2C:0x7D7DF8AE,
                         0x38:0x419A0010, 0x4C:0x9A5E0000, 0x50:0x2F1F0008}.items():
        landmark(image, gate + delta, word)
    flag = image.address(SET_FLAG)
    assert image.pattern(flag, [(0x614A53E8, ALL)]), "setter does not materialize +0xF53E8"
    return [gate, setter, ai, flag]


def world_queue(image):
    entry = image.address(BRIDGE)
    audio = image.call(entry, "?Append@?$BaseEventQueue@UAudioCarDataLoadedEvent")
    queue = image.call(entry, "?GetGuiEventQueue@UpdateOutputBuffer@BrnWorldIO@")
    getter = image.call(entry, GET_FLAG)
    add = image.call(entry, ADD_GUI)
    assert audio < queue < getter < add
    for delta, word in {-0x18:0x419A001C, -0x14:0x38C00004, -0x10:0x93E10054,
                        -0x0C:0x38A00250, -0x08:0x38810054, 0x08:0x2F1F0008}.items():
        landmark(image, add + delta, word)
    flag = image.address(GET_FLAG)
    assert image.pattern(flag, [(0x614A53E8, ALL)]), "getter does not materialize +0xF53E8"
    return [audio, queue, getter, add, flag]


def cache_case(image):
    entry = image.address(GUI_CACHE)
    candidates = image.pattern(entry, [(word, mask) for word, mask in [
        (0x3965FE88, ALL), (0x2B0B00D8, ALL), (0x41990000, 0xFFFF0000),
        (0x3D800000, 0xFFFF0000), (0x398C0000, 0xFFFF0000),
        (0x5560103A, ALL), (0x7C0C002E, ALL), (0x7C0903A6, ALL), (0x4E800420, ALL)]])
    assert len(candidates) == 1, f"event-376 switch (bound 216) {candidates}"
    start = candidates[0]
    table = ((image.word(start + 12) & 0xFFFF) << 16) + signed(image.word(start + 16) & 0xFFFF, 16)
    slot = table + 4 * (592 - 376)
    case = image.word(slot)
    for off, word in enumerate((0x817F0000, 0x3D400001, 0x39200001,
                                0x614A43E0, 0x7D6BF214, 0x7D2B51AE)):
        landmark(image, case + 4 * off, word)
    return [start, table, slot, case, case + 20]


def render_writer(image):
    entry = image.address(RENDER)
    gate = image.call(entry, PLAYING)
    witnesses = image.pattern(entry, [(0x617386C1, ALL)])
    stores = image.pattern(entry, [(0x7ECB99AE, ALL)])
    assert len(witnesses) == len(stores) == 1 and witnesses[0] < gate < stores[0]
    assert any(stores[0] - 24 < pc < stores[0] for pc, _ in
               image.calls_named(entry, "?RenderRaceCar@RaceCarEntityModule@"))
    return [witnesses[0], gate, stores[0]]


def downstream(image):
    entry = image.address(GUI_BRIDGE)
    matches = image.pattern(entry, [(0x2F030250, ALL)])
    assert len(matches) == 1, matches
    switch = matches[0]
    landmark(image, switch + 0x10, 0x38A00250)
    landmark(image, switch + 0x14, 0x38C00004)
    common_tail = image.target(switch + 0x18, image.word(switch + 0x18))
    landmark(image, common_tail, 0x7FE4FB78)  # mr r4,r31, the original payload
    add = common_tail + 4
    assert image.has_call(add, ADD_GUI), f"forward queue call {add:#x}"
    gui = image.address(GUI_UPDATE)
    rec = image.call(gui, GUI_CACHE)
    getter = image.call(gui, GUI_FLAG)
    count = image.call(gui, GUI_COUNT)
    playback = max(pc for pc, _ in image.calls_named(gui, PLAYING) if pc < getter)
    assert rec < playback < getter < count
    assert image.target(playback + 12, image.word(playback + 12)) > count
    assert image.target(getter + 12, image.word(getter + 12)) == count + 4
    gate = image.address(GUI_FLAG)
    assert image.pattern(gate, [(0x614A43E0, ALL)])
    return [switch, add, rec, playback, getter, count]


def lock_order(image):
    entry = image.address(WORLD)
    call = image.call(entry, BRIDGE)
    spine = "?EntityModulePreSceneUpdate@WorldModule@"
    spine_call = image.call(entry, spine)
    producer_call = image.call(image.address(spine), PRE)
    assert spine_call < call
    lock = image.calls_named(entry, RW2)
    unlock = image.calls_named(entry, UNRW2)
    before = max(pc for pc, _ in lock if pc < call)
    after = min(pc for pc, _ in unlock if pc > call)
    assert after - before < 0x90, "World pre-scene lock bracket"
    for pc, word in [(before - 12, 0x7F05C378), (before - 4, 0x7F43D378),
                     (call - 12, 0x7F05C378), (call - 8, 0x7F44D378),
                     (after - 12, 0x7F05C378), (after - 4, 0x7F43D378)]:
        landmark(image, pc, word)  # World output r26 and RaceCar output r24
    for fn, wanted in [(image.address(RW2), (WRITE, READ, READ)),
                       (image.address(UNRW2), (UNREAD, UNREAD, UNWRITE))]:
        found = sorted((pc, name) for name in set(wanted) for pc, _ in image.calls_named(fn, name))
        assert tuple(name for _, name in found) == wanted, (hex(fn), found)
    pre = image.address(PRE)
    read = image.call(pre, READ)
    write = image.call(pre, WRITE)
    unwrite = image.call(pre, UNWRITE)
    unread = image.call(pre, UNREAD)
    assert read < write < image.call(pre, CAR_PASS) < unwrite < unread
    if image.calls_named(pre, SET_FLAG):
        assert write < image.call(pre, SET_FLAG) < unwrite
    gui_frame = image.address("?DoUpdate_GUI@BrnGameModule@")
    world_gui = image.call(gui_frame, "?BridgeWorldToGui@BrnGameModule@")
    gui_lock = max(pc for pc, _ in image.calls_named(gui_frame, "?LockBuffersForIO@CgsModule@")
                   if pc < world_gui)
    gui_unlock = min(pc for pc, _ in image.calls_named(gui_frame, "?UnlockBuffersForIO@CgsModule@")
                     if pc > world_gui)
    for pc, word in [(gui_lock - 8, 0x7EE6BB78), (gui_lock - 4, 0x7FC3F378),
                     (world_gui - 12, 0x7EE5BB78), (world_gui - 8, 0x7FC4F378)]:
        landmark(image, pc, word)  # World output r23, GUI input r30
    assert image.call(image.address("?BridgeWorldToGui@BrnGameModule@"), GUI_BRIDGE)
    return [spine_call, producer_call, before, call, after, read, write, unwrite, unread,
            gui_lock, world_gui, gui_unlock]


COFF_OWNERS = {
    "BrnRaceCarEntityModule-7a7e1565.obj": (PRE, SET_FLAG),
    "WorldBridgeEntityModulesToOutput-429ac799.obj": (BRIDGE, GET_FLAG),
    "BrnGuiCache-b13d1757.obj": (GUI_CACHE,),
}


def compare_coff(image, folder):
    evidence = {}
    for object_name, prefixes in COFF_OWNERS.items():
        path = folder / object_name
        data = path.read_bytes()
        machine, count, _, symbol_at, symbol_count, optional, _ = struct.unpack_from(
            "<HHIIIHH", data)
        assert machine == 0x01F2, object_name
        sections = [struct.unpack_from("<8sIIIIIIHHI", data, 20 + optional + i * 40)
                    for i in range(count)]
        strings = symbol_at + symbol_count * 18
        owners = {}
        index = 0
        while index < symbol_count:
            raw, value, section, _, storage, aux = struct.unpack_from(
                "<8sIhHBB", data, symbol_at + index * 18)
            if section > 0 and storage == 2:
                name = (data[strings + struct.unpack_from("<I", raw, 4)[0]:].split(b"\0", 1)[0]
                        if raw[:4] == b"\0" * 4 else raw.rstrip(b"\0")).decode("ascii")
                for prefix in prefixes:
                    if name.startswith(prefix):
                        assert prefix not in owners, name
                        owners[prefix] = (value, sections[section - 1])
            index += 1 + aux
        assert owners.keys() == set(prefixes), (object_name, list(owners))
        functions = {}
        for prefix, (value, section) in owners.items():
            assert section[0].rstrip(b"\0") == b".text" and value == 0, prefix
            size, raw, reloc_at, reloc_count = section[3], section[4], section[5], section[7]
            relocs = {struct.unpack_from("<I", data, reloc_at + i * 10)[0] & ~3
                      for i in range(reloc_count)}
            entry = image.address(prefix)
            differences = []
            for offset in range(0, size, 4):
                if offset in relocs:
                    continue
                expected, = struct.unpack_from(">I", data, raw + offset)
                linked = image.word(entry + offset)
                if expected != linked:
                    differences.append(f"{entry + offset:#010x}: {linked:08x} != {expected:08x}")
            assert not differences, (object_name, prefix, differences[:8])
            functions[prefix] = {"entry":f"{entry:#010x}", "compared_words":size // 4 - len(relocs),
                                 "relocated_words":len(relocs)}
        evidence[object_name] = {"sha256":digest(path), "functions":functions}
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("seal_dir", type=Path)
    parser.add_argument("--head", required=True)
    parser.add_argument("--pe-sha", required=True)
    parser.add_argument("--map-sha", required=True)
    parser.add_argument("--xex-sha", required=True)
    parser.add_argument("--seal-sha")
    parser.add_argument("--source-repo", type=Path)
    parser.add_argument("--tree")
    parser.add_argument("--expect-base", action="store_true")
    parser.add_argument("--coff-dir", type=Path, help="composed XDK preflight directory")
    args = parser.parse_args()
    paths = {name: args.seal_dir / f"native-game.{ext}"
             for name, ext in {"pe":"pe", "map":"map", "xex":"exe"}.items()}
    expected = {"pe":args.pe_sha, "map":args.map_sha, "xex":args.xex_sha}
    hashes = {name:digest(path) for name,path in paths.items()}
    seal_path = args.seal_dir / "seal.json"
    seal_sha = digest(seal_path)
    if args.seal_sha:
        assert seal_sha == args.seal_sha, (seal_sha, args.seal_sha)
    seal = json.loads(seal_path.read_text(encoding="utf-8-sig"))
    assert seal["head"] == args.head, (seal["head"], args.head)
    assert bool(args.source_repo) == bool(args.tree), "--source-repo and --tree belong together"
    if args.tree:
        tree = subprocess.check_output(["git", "-C", str(args.source_repo), "rev-parse",
                                        args.head + "^{tree}"], text=True).strip()
        assert tree == args.tree, (tree, args.tree)
    assert hashes == expected, (hashes, expected)
    image = Image(paths["pe"], paths["map"])
    checks = {}
    for check in (controls, replay_drain, world_queue, cache_case, render_writer, downstream, lock_order):
        try:
            locations = check(image)
            checks[check.__name__] = {"pass":True, "words":[f"{pc:#010x}:{image.word(pc):08x}"
                                                           for pc in locations]}
        except (AssertionError, ValueError, StopIteration) as error:
            checks[check.__name__] = {"pass":False, "error":str(error)}
    if args.coff_dir:
        try:
            objects = compare_coff(image, args.coff_dir)
            selected = {Path(row["object"]).resolve(): row["sha256"] for row in seal["selected"]}
            for filename, evidence in objects.items():
                path = (args.coff_dir / filename).resolve()
                assert selected.get(path) == evidence["sha256"], f"object outside the seal: {path}"
            checks["coff_postimages"] = {"pass":True, "objects":objects}
        except (AssertionError, ValueError, StopIteration) as error:
            checks["coff_postimages"] = {"pass":False, "error":str(error)}
    missing = {name for name, row in checks.items() if not row["pass"]}
    expected_missing = {"controls","replay_drain","world_queue","cache_case"} if args.expect_base else set()
    print(json.dumps({"head":args.head,"tree":args.tree,"seal_sha256":seal_sha,
                      "hashes":hashes,"checks":checks,
                      "expected_base":args.expect_base,"pass":missing == expected_missing}, indent=2))
    return int(missing != expected_missing)


if __name__ == "__main__":
    raise SystemExit(main())

