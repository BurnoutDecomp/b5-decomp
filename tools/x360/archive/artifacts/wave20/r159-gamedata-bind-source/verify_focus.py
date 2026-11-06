"""Verify each GameData prepare wrapper in the focused PPC listing."""
import json
from pathlib import Path
import re

from build_focus import OUT, REPO, BASE, git, sha

ARTIST_EXPORTS = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")
AUDIT = OUT.parent / "r159-gamedata-bind-order"
CASES = (
    ("PrepareVehicleList", 0x8266C410, 0x8266C810, "AddListResource@VehicleList", 0x230,
     5, "after_destroy", (875, 907, 914, 946)),
    ("PrepareFreeburnChallengeList", 0x8266C088, 0x8266C340, "AddListResource@ChallengeList", 0x22C,
     5, "after_destroy", (716, 769, 776, 803)),
    ("PrepareICEList", 0x8266CEB0, 0x8266D138, "AddListResource@ICEList", 0x234,
     5, "after_destroy", (1257, 1286, 1291, 1311)),
    ("PrepareWheelList", 0x8266D1F8, 0x8266D4AC, "AddListResource@WheelList", 0x238,
     2, "after_destroy", (1383, 1415, 1422, 1446)),
    ("PrepareHudMessages", 0x8266C8E0, 0x8266CB1C, "AddMessages@HudMessageController", 0x244,
     5, "none", (1015, 1045, 1065)),
    ("PreparePopups", 0x8266CBA0, 0x8266CE24, "AddPopupResource@PopupController", 0x65998,
     5, "before_unlock", (1135, 1164, 1167, 1187)),
)


def function(listing, name):
    expr = rf"^(\?{name}@GameDataModule@BrnResource@@\S*)\s+PROC NEAR.*?^\1\s+ENDP"
    match = re.search(expr, listing, re.MULTILINE | re.DOTALL)
    assert match, name
    return [(int(offset, 16), line) for offset, line in
            re.findall(r"^  ([0-9a-f]{5})\t[0-9a-f]{8}\t(.*)$", match[0], re.MULTILINE)]


def marker(rows, text, start=0):
    return next(i for i in range(start, len(rows)) if text in rows[i][1])


def edge(rows, i):
    return f"0x{rows[i][0]:X}"


def main():
    head = git("rev-parse", "HEAD")
    assert git("rev-parse", "HEAD^") == BASE and not git("status", "--porcelain=v1")
    assert git("rev-parse", "refs/heads/codex/experimental-x360-r159-live-update") == BASE
    compile_path = OUT / "compile-receipt.json"
    record = json.loads(compile_path.read_bytes())
    assert record["head"] in (head, BASE)
    for src, digest in record["input_sha256"].items():
        assert sha(REPO / src) == digest, src
    assert len(record["results"]) == 5
    for row in record["results"]:
        name = Path(row["source"]).stem
        assert row["exit_code"] == 0
        assert sha(OUT / f"{name}.obj") == row["ppc_coff_sha256"]
        assert sha(OUT / f"{name}.asm") == row["listing_sha256"]
    listing = (OUT / "BrnGameDataModule.asm").read_text(errors="replace")
    clear = "?Clear@BaseEventReceiverQueue@"
    add = "?AddEvent@?$VariableEventQueue@"
    success = []
    for name, original, original_bind, binder, stage_slot, terminal, clear_order, lines in CASES:
        rows = function(listing, name)
        native_file = (AUDIT / "wheel-art-idb-0x8266D1F8.txt" if name == "PrepareWheelList"
                       else ARTIST_EXPORTS / f"0x{original:08X}.json")
        native = (native_file.read_text() if name == "PrepareWheelList" else
                  json.loads(native_file.read_bytes())["assembly"])
        assert re.search(rf"0x{original_bind:08X}\s+bl\s+.*{binder.split('@')[0]}", native)
        i_alloc = marker(rows, "CreateIOBuffer@UInputBuffer@ResourceIO@")
        i_lock = marker(rows, "?LockForWrite@IOBuffer@", i_alloc)
        i_bind = marker(rows, binder, i_lock)
        i_unlock = marker(rows, "?UnlockForWrite@IOBuffer@", i_bind)
        i_destroy = marker(rows, "DestroyIOBuffer@UInputBuffer@ResourceIO@", i_unlock)
        if name == "PreparePopups":
            assert any(re.search(r"addis\s+r27,r30,6\b", insn) for _, insn in rows[:i_lock])
            assert any(re.search(r"addi\s+r27,r27,22936\b", insn) for _, insn in rows[:i_lock])
            stage_pattern = r"stw\s+r\d+,0\(r27\)"
        else:
            stage_pattern = rf"stw\s+r\d+,{stage_slot:x}h\(r\d+\)"
        stage_store = next(i for i in range(i_bind, i_unlock)
                           if re.search(stage_pattern, rows[i][1], re.I))
        reg = re.search(r"stw\s+(r\d+),", rows[stage_store][1])[1]
        stage_value = next(re.search(rf"li\s+{reg},(\d+)", rows[i][1])[1]
                           for i in range(stage_store - 1, -1, -1)
                           if re.search(rf"li\s+{reg},(\d+)", rows[i][1]))
        assert int(stage_value) == terminal, (name, rows[stage_store], stage_value)
        pending = marker(rows, "?UpdateResourceModule@GameDataModule@", i_destroy)
        assert marker(rows, "?UnlockForWrite@IOBuffer@", i_destroy + 1) < pending
        assert marker(rows, "DestroyIOBuffer@UInputBuffer@ResourceIO@", pending) > pending
        clears = [i for i in range(i_bind, pending) if clear in rows[i][1]]
        assert len(clears) == (0 if clear_order == "none" else 1)
        if clear_order == "before_unlock":
            assert clears[0] < stage_store
        if clear_order == "after_destroy":
            assert clears[0] > i_destroy
        posts = [i for i in range(i_lock, i_bind) if add in rows[i][1]]
        setup_clears = [i for i in range(i_lock, i_bind) if clear in rows[i][1]]
        assert len(posts) == len(setup_clears) == 2
        for i, tag, size in zip(posts, (2, 4), (148, 24)):
            setup = "\n".join(line for _, line in rows[i - 10:i])
            assert re.search(rf"li\s+r5,{tag}\b", setup)
            assert re.search(rf"li\s+r6,{size}\b", setup)
        if clear_order in ("none", "before_unlock"):
            assert setup_clears[0] < posts[0] < setup_clears[1] < posts[1]
        else:
            assert posts[0] < setup_clears[0] < posts[1] < setup_clears[1]
        for line in lines:
            assert any(re.search(rf"li\s+r[35],{line}\b", insn) for _, insn in rows)
        assert sum("FireDataListReplyAssert@" in insn for _, insn in rows) == len(lines) - 1
        if name == "PrepareVehicleList":
            assert marker(rows, "LogVehicleListResource@", i_lock) < i_bind
        success.append({"handler": name, "artist_entry": f"0x{original:08X}",
                        "artist_sha256": sha(native_file), "artist_bind": f"0x{original_bind:08X}",
                        "coff_bind": edge(rows, i_bind), "coff_terminal_store": edge(rows, stage_store),
                        "coff_unlock": edge(rows, i_unlock), "coff_destroy": edge(rows, i_destroy),
                        "coff_terminal_receiver_clear": edge(rows, clears[0]) if clears else None,
                        "terminal_stage": terminal, "original_assert_lines": lines})
    receipt = {"commit": head, "tree": git("rev-parse", "HEAD^{tree}"), "parent": BASE,
               "compile_receipt_sha256": sha(compile_path), "focused_ppc_compiles": 5,
               "wrapper_offsets_are_function_relative": True, "stages": success,
               "scope": "Focused XDK PPC listing comparison against ARTIST. Full link and runtime pending."}
    (OUT / "RECEIPT.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("Verified 6 GameData PPC terminal, request and pending paths")


if __name__ == "__main__":
    main()
