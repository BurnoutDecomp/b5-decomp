"""Check CompleteLoading entry order against ARTIST and integrated R139."""
import json
from pathlib import Path

here = Path(__file__).resolve().parent
wave = here.parent
helper = wave / "r128-batch/verify_matches.py"
ns = {"__file__": str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:", 1)[0], ns)

source = "src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.cpp"
r139 = "e53482ef4d51dde3448310835cbcbdedfa1b4e16"
enter = "?OnEnter@MainGameFlowStateCompleteLoading@@UAAXXZ"
pause = "?gBrnScriptedLoadPaused@@3_NA"
game = "?gpBurnoutGame@@3PAVBrnGameModule@BrnGame@@A"
video = "?SetVideoState@GameMainFlowController@BrnGameMainFlowController@@QAAX_N@Z"
before_folder = wave / "r139-intake"


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def object_for(report):
    item, = [row for row in read(report)["results"] if row["source"] == source]
    assert item["passed"] and item["machine"] == 0x1F2
    return Path(item["object"])


def function(obj, symbol):
    body, relocs = ns["collect"](obj, symbol)
    blob = obj.read_bytes()
    references = [(off, kind, ns["object_symbol"](blob, index)[0])
                  for off, index, kind in relocs if kind != 18]
    return body, references


def words(body, offset, expected):
    data = bytes.fromhex(expected)
    assert body[offset:offset + len(data)] == data, (hex(offset), expected)


assert read(before_folder / "commit-binding.json")["head"] == r139
before_obj = object_for(before_folder / "native-selected/compile-results.json")
after_obj = object_for(here / "compile-results.json")
before, prior_refs = function(before_obj, enter)
after, refs = function(after_obj, enter)

artist = ns["original"](0x823AABD0, 0x44)
words(artist, 0, "3d60009a 3d4082fb 61690644 3d60009a 39000003 616706b9 39600000 "
      "38c00001 996ae28e 3d608301 814b02d0 7d0a492e 816b02d0 7ccb39ae "
      "39600000 99630004 4e800020")
words(before, 0x30, "7d0a492e 39400000 99430004")
assert (0x48, 6, video) in prior_refs
assert (0x70, 6, "??6StrStreamBase@CgsDev@@QAAAAU01@PBD@Z") in prior_refs

words(after, 0x10, "3d60009a 3d400000 61690644 39600000 39000003 7c7f1b78 38800001 "
      "996a0000 3d600000 814b0000 7d0a492e 816b0000 3c6b009a 38630664 "
      "4bffffb9 39600000 997f0004")
assert refs == [(0x14, 16, pause), (0x2C, 17, pause), (0x30, 16, game),
                (0x34, 17, game), (0x3C, 17, game), (0x48, 6, video)]
words(after, 0x64, "4e800020")
assert len(after) == 0x68
address, linked = ns["linked_reader"](wave / "r137-link")
words(linked(address(video), 8), 0, "98830055 4e800020")

neighbors = [
    "?OnLeave@MainGameFlowStateCompleteLoading@@UAAXXZ",
    "?Update@MainGameFlowStateCompleteLoading@@UAAXXZ",
    "?OnEnter@MainGameFlowStateMemoryCard@@UAAXXZ",
    "?OnLeave@MainGameFlowStateMemoryCard@@UAAXXZ",
    "?Update@MainGameFlowStateMemoryCard@@UAAXXZ",
    "?OnEnter@MainGameFlowStateCheckDiskSpace@@UAAXXZ",
]
for symbol in neighbors:
    assert function(before_obj, symbol) == function(after_obj, symbol), symbol

receipt = {
    "scope": "ARTIST bytes, integrated R139 COFF baseline, and targeted R141 SDK COFF",
    "source": source,
    "r139_commit": r139,
    "artist_va": "0x823AABD0",
    "artist_bytes": len(artist),
    "r139_entry_bytes": len(before),
    "r141_entry_bytes": len(after),
    "entry_sequence": [
        {"effect": "unpark scripted load", "artist_va": "0x823AABF0", "candidate_offset": "0x2C"},
        {"effect": "request GUI stage 3", "artist_va": "0x823AABFC", "candidate_offset": "0x38"},
        {"effect": "set video state", "artist_va": "0x823AAC04", "candidate_offset": "0x48"},
        {"effect": "clear collision latch at this+4", "artist_va": "0x823AAC0C", "candidate_offset": "0x50"},
    ],
    "candidate_call": video,
    "r137_linked_video_setter_va": hex(address(video)),
    "game_video_flag_offset": "0x9A06B9",
    "neighbor_functions_equal": neighbors,
    "r139_object": str(before_obj),
    "r139_object_sha256": ns["sha"](before_obj),
    "r141_object": str(after_obj),
    "r141_object_sha256": ns["sha"](after_obj),
}
output = here / "R141_COMPLETE_LOADING_COFF.json"
output.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(output, ns["sha"](output))

