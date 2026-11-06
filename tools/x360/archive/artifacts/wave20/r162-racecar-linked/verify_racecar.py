
"""Verify RaceCar PPC against the final pinned R162 PE and its selected objects."""
import argparse
import json
from pathlib import Path
import runpy
import sys

OUT = Path(__file__).resolve().parent
WAVE = OUT.parent
CANDIDATE = "8b9f64f5ebb873ba5b6577734d1afb9881176c30"
TREE = "c9a45a7234193c374f89d1161d756928f0ba94b7"
PARENT = "808f548465be6e2c81967f4ea8af1419f6f651fc"
COMPOSED_SHA = "c1f2487281c5712858a7ef87a4705de333e486b7fc3cfaf12fe6c01eabb8ce53"
BINDING_SHA = "322625e0f2e885d2f463ea7bc8028619e34eae73345d32566ee722f11645c450"
H = runpy.run_path(str(WAVE / "r160-gui-linked/verify_gui.py"))
IMAGE, COFF, load, sha, digest, word, canonical, pe_bytes, split_address, tree, blob, inspect = (
    H[key] for key in ("IMAGE", "COFF", "load", "sha", "digest", "word", "canonical",
                       "pe_bytes", "split_address", "tree", "blob", "inspect")
)
map_symbols = H["TOOLS"]["PRIOR"]["HELPER"]["map_symbols"]
sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
from x360rd import rd, u32

RACE = "src/GameSource/World/EntityModules/RaceCarEntityModule/"
C = RACE + "BrnRaceCarEntityModule.cpp"
R = RACE + "BrnRaceCarEntityModule_Range.cpp"
T = RACE + "BrnRaceCarEntityModule_wS34_00.cpp"
S = RACE + "BrnRaceCarComponentStreamers.cpp"
W = "src/GameSource/World/BrnWorldModule.cpp"
BASE = "src/GameShared/GameClasses/Module/CgsModuleSingleBuffered.cpp"
PREPARE = ("?Prepare@RaceCarEntityModule@BrnWorld@@QAA_NPAUOutputBuffer_Prepare"
           "@RaceCarEntityModuleIO@2@UResourceHandle@CgsResource@@@Z")
RNG = "?mRandom@BrnWorld@@3VRandom@CgsNumeric@@A"
VTABLE = "??_7RaceCarEntityModule@BrnWorld@@6B@"
REGISTER = "?Register@DebugComponent@"
SET_SEED, UINT = "?SetSeed@Random@", "?RandomUInt@Random@"
CHECKS = {
    "world_prepare": (W, "?Prepare@WorldModule@BrnWorld@@",
        {0x4B4: PREPARE}, {0x4A8: 0x616B1B88, 0x4AC: 0x7FA4EB78, 0x4B0: 0x7CBF582A}),
    "base_construct": (BASE, "?Construct@ModuleSingleBuffered@CgsModule@@", {}, {}),
    "race_construct": (C, "?Construct@RaceCarEntityModule@BrnWorld@@", {
        0x14: "?Construct@ModuleSingleBuffered@", 0x7C: "?Construct@RaceCarStreamer@",
        0x8C: "?Construct@PlaceOnTrackManager@",
    }, {0x18: 0x39600002, 0x2C: 0x917F0230, 0x84: 0x7FE4FB78, 0x88: 0x38637850,
        0x178: 0x3BA00001, 0x3D8: 0x9BBF0004}),
    "race_prepare": (C, PREPARE, {
        0x23C: "?Prepare@PowerParkingManager@", 0x248: REGISTER, 0x258: SET_SEED,
        0x264: REGISTER, 0x270: REGISTER, 0x27C: "?Prepare@ModuleSingleBuffered@",
        0x2A4: "?LoadGlobalResources@RaceCarEntityModule@", 0x2E4: "?Prepare@RaceCar@",
        0x304: "?Prepare@ActiveRaceCar@", 0x324: SET_SEED, 0x334: SET_SEED,
        0x45C: "?ClearAllActiveRaceCarToPlayerScoringMappings@", 0x480: "?Prepare@RaceCarStreamer@",
    }, {
        0x10: 0x7CB82B78, 0x24: 0xFB010120, 0xD8: 0x39206DC2, 0xDC: 0x3AA00001, 0xE0: 0x3BC00000,
        0xEC: 0x65378FE0, 0x154: 0xFB1F023C, 0x244: 0x386380F0, 0x250: 0x7EE4BB78,
        0x278: 0x92BF022C, 0x280: 0x546B063E, 0x284: 0x2B0B0000, 0x288: 0x419A0228,
        0x28C: 0x39600002, 0x294: 0x917F022C,
        0x2C4: 0x92BF022C, 0x2C8: 0x3B60FFFF, 0x2CC: 0x93DF0230, 0x2D0: 0x616B82F8,
        0x2D8: 0x3BA00023, 0x2DC: 0x7F7F592E, 0x2FC: 0x3BA00008,
        0x31C: 0x7EE4BB78, 0x320: 0x38638460, 0x32C: 0x7EE4BB78, 0x330: 0x38638490,
        0x42C: 0x39200007, 0x434: 0x394A83A8, 0x448: 0xFBCA0000, 0x44C: 0x394A0008,
        0x450: 0x4200FFF8, 0x458: 0x93CA0000, 0x468: 0x616B8438, 0x46C: 0x614A8434,
        0x474: 0x38631100, 0x478: 0x7CBF582E, 0x47C: 0x7C9F502E, 0x484: 0x38600001,
        0x4B0: 0x38600000,
    }),
    "global_load_completion": (C, "?LoadGlobalResources@RaceCarEntityModule@BrnWorld@@",
        {}, {0x37C: 0x614A8438, 0x380: 0x7D7F512E, 0x384: 0x38600001}),
    "race_release": (T, "?Release@RaceCarEntityModule@BrnWorld@@",
        {0x5C: "?Release@ModuleSingleBuffered@"}, {
            0x18: 0x3BC00001, 0x1C: 0x817F0230, 0x20: 0x2B0B0002, 0x24: 0x41980030,
            0x28: 0x419A0044, 0x4C: 0x38600000, 0x58: 0x93DF0230, 0x60: 0x546B063E,
            0x64: 0x2B0B0000, 0x68: 0x419AFFE4, 0x70: 0x93DF0230, 0xA0: 0x917F022C,
        }),
    "race_destruct": (T, "?Destruct@RaceCarEntityModule@BrnWorld@@", {
        0x18: "?Destruct@ModuleSingleBuffered@", 0x24: "?Destruct@RaceCarEntityModuleDebugComponent@",
        0x30: "?Destruct@RaceCarStreamer@",
    }, {}),
    "colour_draws": (R, "?GetRandomCarColour@RaceCarEntityModule@BrnWorld@@", {0x3C: UINT, 0x5C: UINT}, {}),
    "range_draw": (R, "?UpdateInAndOutOfRangeCars@RaceCarEntityModule@BrnWorld@@", {0x220: UINT}, {}),
    "streamer_prepare": (S, "?Prepare@RaceCarStreamer@BrnWorld@@", {
        0x24: "?BeginAssert@", 0x3C: "?FireAssert@", 0x40: "?EndAssert@",
    }, {0x14: 0x7C9F2378, 0x20: 0x409A0024, 0x2C: 0x38A00057, 0x44: 0x93FE0000}),
}
RNG_PAIRS = {"race_prepare": (0x24C, 0x254), "colour_draws": (0x10, 0x20), "range_draw": (0xDC, 0xE0)}
ARTIST_EDGES = {
    0x827D58C4: 0x82303E78, 0x822FD8BC: 0x8286E768, 0x822FD940: 0x822EA188,
    0x82304054: 0x828331D8, 0x823040D8: 0x8286E7A0, 0x823040E4: 0x8230442C,
    0x822BD3D4: 0x8286E970, 0x822BD3E0: 0x822BD3EC, 0x822F3DD0: 0x8286E040,
}
ARTIST_WORDS = {
    0x827D58C0: 0x7CBF582A, 0x82303E88: 0x7CBC2B78, 0x82303E9C: 0xFB810110,
    0x82303F7C: 0xFB9E023C, 0x822FD8D4: 0x917E0230, 0x822FE3D4: 0x997F0004,
    0x82303ED0: 0x39206DC2, 0x82303ED8: 0x65388FE0, 0x82304120: 0x933E022C,
    0x82304128: 0x93FE0230, 0x823041D4: 0x396B8460, 0x82304230: 0x396B8490,
    0x82304398: 0x38A00007, 0x823043A8: 0xF88A0000, 0x823043B8: 0x908A0000,
    0x82304400: 0x7FFE592E,
}
ARTIST_RNG = [(0x82304058, 0x82304060), (0x822EA098, 0x822EA0A8), (0x822FF9B8, 0x822FF9C0)]


def function(row, prefix):
    return COFF(Path(row["object"]).read_bytes()).function(prefix)


def actual_branch(image, va, offset):
    pc = va + offset
    return image.target(pc, image.word(pc))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root-pins", type=Path, required=True)
    parser.add_argument("--root-pins-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    assert sha(args.root_pins) == args.root_pins_sha256.lower()
    root = load(args.root_pins)
    head, pins = root["head"], root["pins"]
    assert root["tree"] == tree(head) == tree(CANDIDATE) == TREE
    assert root["parent"] == H["TOOLS"]["git"]("rev-parse", head + "^").decode().strip() == PARENT
    for item in pins.values():
        assert sha(Path(item["path"])) == item["sha256"]
    seal, invocation, result, completion, xex, diagnostics = (
        load(Path(pins[key]["path"])) for key in ("seal", "invocation", "result", "completion", "image", "diagnostics"))
    assert seal["head"] == completion["head"] == xex["head"] == head
    assert invocation["sealed_sha256"] == pins["seal"]["sha256"] and invocation["objects"] == seal["selected"]
    assert result["exit_code"] == 0 and result["all_post_checks_pass"]
    assert completion["passed"] == completion["requested"] == 3710 and completion["failed"] == 0
    assert diagnostics["unresolved_messages"] == 0 and not diagnostics["duplicate_messages"]
    assert xex["magic"] == "XEX2" and xex["image_sha256"] == pins["xex"]["sha256"]
    outputs = {Path(path).name: row for path, row in result["outputs"].items()}
    assert all(outputs[Path(pins[key]["path"]).name]["sha256"] == pins[key]["sha256"]
               for key in ("pe", "map", "xex"))

    binding_path = WAVE / "r162-batch/TARGETED_OBJECT_BINDING.json"
    composed_path = WAVE / "r162-batch/COMPOSED_PPC_CHECK.json"
    assert sha(binding_path) == BINDING_SHA and sha(composed_path) == COMPOSED_SHA
    binding, composed = load(binding_path), load(composed_path)
    assert binding["head"] == composed["head"] == CANDIDATE and binding["tree"] == composed["tree"] == TREE
    assert composed["targeted_binding_sha256"] == BINDING_SHA
    before_rows = {row["source"]: {"object": row["artifacts"]["object"]["path"],
                                   "sha256": row["artifacts"]["object"]["sha256"],
                                   "git_blob": row["git_blob"]} for row in binding["results"]}
    rows = {row["source"]: row for row in invocation["objects"]}
    source_root = Path(seal["source_root"])
    sources = {Path(row["path"]).relative_to(source_root).as_posix().lower(): row for row in seal["sources"]
               if Path(row["path"]).is_relative_to(source_root)}
    map_path, pe_path = (Path(pins[key]["path"]) for key in ("map", "pe"))
    entries = map_symbols(map_path.read_bytes())
    image = IMAGE(pe_path, map_path)

    for address, expected in ARTIST_WORDS.items():
        assert u32(address) == expected, hex(address)
    for address, target in ARTIST_EDGES.items():
        assert IMAGE.target(None, address, u32(address)) == target, hex(address)
    for high, low in ARTIST_RNG:
        assert split_address(rd(high, low - high + 4), 0, low - high) == 0x82FAD2B0

    final = {}
    for label, spec in CHECKS.items():
        source, prefix, _, _ = spec
        before = before_rows[source]
        assert before["git_blob"] == blob(CANDIDATE, source) == blob(head, source)
        assert sha(Path(before["object"])) == before["sha256"]
        _, code, refs = function(before, prefix)
        item = inspect(label, spec, rows, head, (image, entries, sources))
        assert item["coff_masked_body_sha256"] == digest(canonical(code, refs)), label
        final[label] = item
    base_va = int(final["base_construct"]["linked_va"], 16)
    assert pe_bytes(image, base_va, 56) == rd(0x8286E768, 56)

    [rng_va] = {address for address, owner in entries[RNG]
                if owner.lower() == Path(final["race_prepare"]["object"]).name.lower()}
    rng_sites = {}
    for label, (high, low) in RNG_PAIRS.items():
        source, prefix, _, _ = CHECKS[label]
        _, _, refs = function(rows[source], prefix)
        assert refs[high][0] == 16 and refs[low][0] == 17 and refs[high][1] == refs[low][1]
        assert refs[high][1].name == RNG
        va = int(final[label]["linked_va"], 16)
        assert split_address(pe_bytes(image, va, low + 4), high, low) == rng_va
        rng_sites[label] = {"hi_va": hex(va + high), "lo_va": hex(va + low)}

    world_owner = Path(final["world_prepare"]["object"]).name
    [table] = {address for address, owner in entries[VTABLE] if owner.lower() == world_owner.lower()}
    coff = COFF(Path(final["world_prepare"]["object"]).read_bytes())
    slot_targets = {0: final["race_construct"]["symbol"], 4: "?Prepare@ModuleSingleBuffered@CgsModule@@UAA_NXZ",
                    8: final["race_release"]["symbol"], 12: final["race_destruct"]["symbol"]}
    vslots = {}
    for offset, symbol in slot_targets.items():
        assert coff.vtable_slot(VTABLE, offset) == symbol
        assert image.word(table + offset) in {address for address, _ in entries[symbol]}
        vslots[hex(offset)] = {"symbol": symbol, "target_va": hex(image.word(table + offset))}

    retry = {}
    for label, site, dest in (("race_prepare", 0x288, 0x4B0), ("race_release", 0x68, 0x4C),
                              ("race_release", 0x28, 0x6C), ("streamer_prepare", 0x20, 0x44)):
        va = int(final[label]["linked_va"], 16)
        assert actual_branch(image, va, site) == va + dest
        retry[f"{label}+{hex(site)}"] = hex(va + dest)
    stream = int(final["streamer_prepare"]["linked_va"], 16)
    code = pe_bytes(image, stream, 0x44)
    for high, low, original in ((0x28, 0x30, 0x820167A0), (0x34, 0x38, 0x82016784)):
        raw = rd(original, 192).split(bytes(1))[0] + bytes(1)
        assert pe_bytes(image, split_address(code, high, low), len(raw)) == raw

    receipt = {"head": head, "tree": TREE, "parent": PARENT, "candidate": CANDIDATE,
               "root_pins_sha256": args.root_pins_sha256.lower(), "pins": pins, "binding_sha256": BINDING_SHA,
               "composed_ppc_sha256": COMPOSED_SHA, "functions": final, "race_vtable": vslots,
               "shared_rng_va": hex(rng_va), "rng_sites": rng_sites, "retry_and_assert_continuations": retry,
               "artist_edges": {hex(site): hex(target) for site, target in ARTIST_EDGES.items()},
               "scope": "Pinned selected COFF, map and linked PE. Strict link attested by pins. Runtime unverified."}
    text = json.dumps(receipt, indent=2) + "\n"
    if args.output.exists():
        assert args.output.read_text(encoding="utf-8") == text
    else:
        with args.output.open("x", encoding="utf-8") as file:
            file.write(text)
    print("PASS", len(final), "RaceCar-related linked functions", sha(args.output))


if __name__ == "__main__":
    main()

