"""Bind focused PPC evidence to the clean private R159 LiveUpdate commit."""
import json
from pathlib import Path
import re
import struct
import sys

from build_focus import OUT, REPO, BASE, git, sha

sys.path.insert(0, "D:/Reverse/BP-Decomp_Workflow/tools/re")
import x360rd


def read_coff_vtable(path):
    data = path.read_bytes()
    machine, section_count, _, symoff, symbol_count, optsize, _ = struct.unpack_from("<HHIIIHH", data)
    assert machine == 0x1F2 and optsize == 0
    string_base = symoff + 18 * symbol_count
    names = {}
    indices = {}
    i = 0
    while i < symbol_count:
        pos = symoff + 18 * i
        name = data[pos:pos + 8]
        if not any(name[:4]):
            start = string_base + int.from_bytes(name[4:], "little")
            name = data[start:data.index(0, start)]
        else:
            name = name.rstrip(b"\0")
        value, section = struct.unpack_from("<Ih", data, pos + 8)
        names[i] = name.decode("ascii")
        indices[names[i]] = (value, section)
        i += 1 + data[pos + 17]

    table_value, table_section = indices["??_7LiveUpdateModule@CgsResource@@6B@"]
    assert 1 <= table_section <= section_count and table_value == 0
    header = 20 + (table_section - 1) * 40
    size, _, relocs = struct.unpack_from("<III", data, header + 16)
    count, = struct.unpack_from("<H", data, header + 32)
    assert size == 0x48 and count == 18
    result = []
    for i in range(count):
        offset, symbol, kind = struct.unpack_from("<IIH", data, relocs + 10 * i)
        assert offset == 4 * i and kind == 2
        result.append(names[symbol])
    return result


def function(listing, symbol):
    pattern = r"^(" + symbol + r"\S*) PROC NEAR.*?^\1 ENDP"
    match = re.search(pattern, listing, re.MULTILINE | re.DOTALL)
    assert match, symbol
    return match[0]


def ordered(block, markers):
    pos = 0
    for marker in markers:
        pos = block.index(marker, pos) + len(marker)


def main():
    head = git("rev-parse", "HEAD")
    assert git("rev-parse", "HEAD^") == BASE
    assert git("status", "--porcelain=v1") == ""
    assert git("rev-parse", "refs/heads/codex/experimental-x360-r156-native-batch-allocator-fix") == (
        "9708f3b6910954ea62a0a118405b4f9b23c163c6")
    compile_path = OUT / "compile-receipt.json"
    compiled = json.loads(compile_path.read_bytes())
    assert compiled["head"] in (head, BASE)
    for source, digest in compiled["input_sha256"].items():
        assert sha(REPO / source) == digest, source
    assert len(compiled["results"]) == 5
    for row in compiled["results"]:
        name = row["source"].rsplit("/", 1)[-1][:-4]
        assert row["exit_code"] == 0
        assert sha(OUT / f"{name}.obj") == row["ppc_coff_sha256"]
        assert sha(OUT / f"{name}.asm") == row["listing_sha256"]

    artist_base = [x360rd.u32(0x820CE500 + i * 4) for i in range(16)]
    artist = [x360rd.u32(0x820CE6D0 + i * 4) for i in range(18)]
    assert artist == [artist_base[0], 0x828E65D8, 0x828E6680, 0x828E6708,
                      *artist_base[4:], 0x828E6548, 0x82903128]
    assert x360rd.u32(artist[4]) == 0x4E800020
    expected_names = ("Construct", "Prepare", "Release", "Destruct", "Update",
                      "SetMultiThreaded", "LockForInput", "UnlockForInput",
                      "LockForOutput", "UnlockForOutput", "DestroyInputDataStructure",
                      "DestroyOutputDataStructure", "PrepareDataStructures",
                      "ReleaseDataStructures", "CreateInputDataStructure",
                      "CreateOutputDataStructure", "Construct", "Update")
    vtable = read_coff_vtable(OUT / "BrnGameDataModule.obj")
    for i, (symbol, name) in enumerate(zip(vtable, expected_names)):
        owner = "LiveUpdateModule@CgsResource" if i in (1, 2, 3, 16, 17) else "ModuleSingleBuffered@CgsModule"
        assert symbol.startswith(f"?{name}@{owner}@@"), (i, symbol)
    assert vtable[16].endswith("@@UAAXPAVGameTalk@CgsGameTalk@@@Z")
    assert vtable[17].endswith("@@UAAXPAUIOBuffer@CgsModule@@@Z")

    module = (OUT / "BrnGameDataModule.asm").read_text(errors="replace")
    # Match emitted instructions only so source comments cannot satisfy a check.
    instr = lambda block: "\n".join(line for line in block.splitlines()
                                    if re.match(r"^  [0-9a-f]{5}\t[0-9a-f]{8}\t", line))
    construct = instr(function(module, re.escape("?Construct@GameDataModule@BrnResource@@")))
    update = instr(function(module, re.escape("?Update@GameDataModule@BrnResource@@")))
    create = instr(function(module, re.escape("??$CreateIOBuffer@UOutputBuffer@LiveUpdateIO@")))
    destroy = instr(function(module, re.escape("??$DestroyIOBuffer@UOutputBuffer@LiveUpdateIO@")))
    ordered(construct, ("38638500", "816b0040", "4e800421"))
    ordered(update, ("?Update@GameTalk@", "?LockForWrite@IOBuffer@", "?LockForRead@IOBuffer@",
                     "387b0009", "?Construct@FileSystemStatusInterface@", "93fb0004",
                     "?SetAllocatorList@OutputBuffer@", "??$CreateIOBuffer@UOutputBuffer@LiveUpdateIO@",
                     "38638500", "816b0044", "4e800421", "?LockForRead@IOBuffer@",
                     "?GetSt@OutputBuffer@LiveUpdateIO@CgsResource@@QBAPBEXZ",
                     "?SetLiveUpdateStatus@OutputBuffer@GameDataIO@", "?UnlockForWrite@IOBuffer@",
                     "?LockForWrite@IOBuffer@", "?GetOutputQueue@OutputBuffer@LiveUpdateIO@CgsResource@@QBAPBV",
                     "?GetResourceQueue@InputBuffer@ResourceIO@", "??$Append@$0CAA@$0BA@@",
                     "?UnlockForRead@IOBuffer@", "??$DestroyIOBuffer@UOutputBuffer@LiveUpdateIO@",
                     "?GetRequestInterface@InputBuffer@GameDataIO@"))
    ordered(create, ("38800218", "?Alloc@IOBufferStack@", "?Construct@OutputBuffer@LiveUpdateIO@"))
    ordered(destroy, ("?Destruct@IOBuffer@", "38a00218", "?Free@IOBufferStack@"))

    output = (OUT / "CgsLiveUpdateModuleIO_OutputBuffer.asm").read_text(errors="replace")
    assert "387f0214" in instr(function(output, re.escape("?GetSt@OutputBuffer@LiveUpdateIO@")))
    assert "387f0004" in instr(function(output, re.escape("?GetOutputQueue@OutputBuffer@LiveUpdateIO@")))
    worker = (OUT / "CgsLiveUpdateModule.asm").read_text(errors="replace")
    worker_update = instr(function(worker, re.escape("?Update@LiveUpdateModule@CgsResource@@")))
    ordered(worker_update, ("38c00094", "38a00002", "?AddEvent@?$VariableEventQueue@$0CAA",
                            "?SetStatus@OutputBuffer@LiveUpdateIO@"))
    receipt = {"commit": head, "tree": git("rev-parse", "HEAD^{tree}"), "parent": BASE,
               "compile_receipt_sha256": sha(compile_path), "focused_ppc_compiles": 5,
               "artist_image": {"path": x360rd.RAW, "sha256": sha(Path(x360rd.RAW))},
               "artist_vtable": "0x820CE6D0", "artist_base_vtable": "0x820CE500",
               "vtable": [{"offset": i * 4, "artist_target": f"0x{target:08X}", "coff_symbol": symbol}
                          for i, (target, symbol) in enumerate(zip(artist, vtable))],
               "game_data_construct": {"artist": "0x82671CE4..0x82671CFC", "coff": "0x15C..0x174"},
               "game_data_live_update": {"artist": "0x82674808..0x826748C4", "coff": "0x60..0x120"},
               "alloc_free_size": 0x218, "live_update_status_offset": 0x214,
               "live_update_queue_offset": 4, "request_event": 2, "request_size": 148,
               "scope": "Exact commit inputs, focused XDK PPC COFF and original ARTIST image. Full link and runtime pending."}
    (OUT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Verified 18 LiveUpdate COFF vtable relocations and emitted PPC bridge edges")


if __name__ == "__main__":
    main()
