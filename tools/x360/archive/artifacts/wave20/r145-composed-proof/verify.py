"""Verify composed R145 CIH and constant-table callback PPC COFF."""

import hashlib
import json
import re
import runpy
import struct
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
COMPOSED = WAVE / "r145-composed-preflight"
CIH_PROOF = WAVE / "r145-cih-proof"
CIH_COMPONENT = WAVE / "r145-apt-cih-flags"
CALLBACK_COMPONENT = WAVE / "r150-apt-constant-callback"
HEAD = "5295d92753407372620b53c4674c60198b13bb97"
TREE = "2242c25d9742f1f64dec98c6c529a90ecb8e0ca4"
PARENT = "b956c18869bdde471088c9a640ea34fb41178f06"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load(path, digest=None):
    data = path.read_bytes()
    if digest:
        assert sha(data) == digest, path
    return json.loads(data)


def stable(refs):
    return {i: re.sub(r"\?A0x[0-9a-f]{8}@", "?anonymous@", name) for i, name in refs.items()}


report_path = COMPOSED / "compile-results.json"
report = load(report_path)
rows = {Path(row["source"]).stem: row for row in report["results"]}
assert report["state"] == "complete" and (report["requested"], report["passed"], report["failed"]) == (11, 11, 0)
assert len(rows) == 11 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows.values())
src_arg = next(arg for arg in next(iter(rows.values()))["command"] if arg.startswith("/I"))
assert all(src_arg in row["command"] for row in rows.values())
checkout = Path(src_arg[2:]).parent


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", "-C", str(checkout), *args], text=True)


assert git("rev-parse", "HEAD", "HEAD^{tree}", "HEAD^").splitlines() == [HEAD, TREE, PARENT]
assert not git("diff", "HEAD", "--name-only")
paths = {stem: Path(row["object"]) for stem, row in rows.items()}
assert all(path.parent == COMPOSED for path in paths.values())

cih_digest = "5d552a88aac2c84c264d8ca33618f008cdb51b7dafd4c5d029929dca8e16adce"
cih_receipt = load(CIH_PROOF / "R145_CIH_COFF.json", cih_digest)
verifier = CIH_PROOF / "verify.py"
assert sha(verifier.read_bytes()) == "acbddcb52f1492bd2d1b395c9b45de3289d50f9997c04a647fc9cdccf25796c7"
cih = runpy.run_path(str(verifier))
coff = cih["coff"]

artist = (WAVE / "native-xenia-smoke/run-r74-artist-no-trace/game/native-game.xex").read_bytes()
assert sha(artist) == cih_receipt["artist_sha256"]
payload = struct.unpack_from(">I", artist, 8)[0]
image_base = payload - 0x82000000
assert artist[payload:payload + 2] == b"MZ"
cases = {
    name: cih["verify_leaf"](name, *coff(paths["AptAnimationTarget" if "RemList" in name else "AptCIH"],
                                       f"?{name}@AptCIH@@"), candidate=True)
    for name in cih_receipt["candidate_cases"]
}
assert cases == cih_receipt["candidate_cases"]
consumers = cih["verify_consumers"](paths, artist, image_base)
assert consumers == cih_receipt["consumers"]

compared = {}


def same(stem, base, names):
    for name in names:
        old, old_refs = coff(base, name)
        new, new_refs = coff(paths[stem], name)
        assert old == new and stable(old_refs) == stable(new_refs), (stem, name)
        compared[name] = {"bytes": len(new) * 4, "relocations": len(new_refs)}


# Compare other direct CIH readers/writers in the focused gate, in addition to
# rerunning the independent bit and branch tests on the composed objects.
for stem, names in {
    "AptAnimationTarget": ("?AptAnimationTargetAddToRemList@@", "?CleanRemList@AptAnimationTarget@@",
                           "?RunActions@AptAnimationTarget@@"),
    "AptKey": ("?sMethod_addListener@AptKey@@",),
    "AptLinker": ("?ConvertToZombie@AptLinker@@", "?Load@AptLinker@@", "?Update@AptLinker@@"),
}.items():
    frozen = CIH_COMPONENT / f"{stem}.obj"
    assert sha(frozen.read_bytes()) == cih_receipt["objects_sha256"][stem]
    same(stem, frozen, names)

# Preserve the R144 providers used by the class-flag and constructor fixes.
r144 = WAVE / "r144-link"
assert load(r144 / "image-verification.json")["head"] == PARENT
selected = {Path(row["source"]).stem: row for row in load(r144 / "invocation.json")["objects"]}
for stem, names in {
    "AptObject": ("?GetHasClass@AptObject@@", "?SetHasClass@AptObject@@", "?SetImplementedObjects@AptObject@@",
                  "?GetImplementedObjects@AptObject@@", "?DoesImplementObject@AptObject@@"),
    "AptValueFindChild": ("?AptResolveSpecialName@@",),
}.items():
    old = Path(selected[stem]["object"])
    assert sha(old.read_bytes()) == selected[stem]["sha256"]
    same(stem, old, names)
find_child, find_refs = coff(paths["AptValueFindChild"], "?AptResolveSpecialName@@")
[get_ctor] = [i for i, name in find_refs.items() if name.startswith("?IsInCtor@AptCIH@@")]
assert find_child[get_ctor] & 0xFC000003 == 0x48000001

callback_digest = "9108fd56c61e5ea13179cbc8c13d3e48535433ddca68434306097dd4fa3ac6d0"
callback = load(CALLBACK_COMPONENT / "object-proof.json", callback_digest)
artist_digest = "3bf9d2cfc8ef2398b6434a715131d97442e9512950bbb5a3434e7a8f034a9fe5"
artist_callback = load(CALLBACK_COMPONENT / "artist-proof.json", artist_digest)
assert artist_callback["image_sha256"] == cih_receipt["artist_sha256"]
for address, word in (artist_callback["assignment"] | artist_callback["dispatch"] |
                      {artist_callback["target"]["address"]: artist_callback["target"]["word"]}).items():
    assert struct.unpack_from(">I", artist, image_base + int(address, 16))[0] == int(word, 16)

construct_name = "?ConstructApt@AptAux@CgsGui@@"
free_name = "?FreeConstantTable@AptCallbackFile@CgsGui@@"
load_name = "?CompleteLoad@AptLoader@@"
for stem, names in {"CgsAptAux": (construct_name, free_name), "AptLoader": (load_name,)}.items():
    old = CALLBACK_COMPONENT / f"{stem}.obj"
    assert sha(old.read_bytes()) == callback["objects_sha256"][old.name]
    same(stem, old, names)

construct, refs = coff(paths["CgsAptAux"], construct_name)
assert refs[0x200 // 4].startswith(free_name)
assert construct[0x200 // 4:0x208 // 4] == (0x395A0000, 0x914B0028)
assert coff(paths["CgsAptAux"], free_name)[0] == (0x4E800020,)
complete, refs = coff(paths["AptLoader"], load_name)
assert all(complete[int(offset, 16) // 4] == int(word, 16)
           for offset, word in callback["complete_load_words"].items())
assert refs[0x84 // 4] == refs[0x8C // 4] == "?gAptFuncs@@3UAptUserFunctions@@A"
assert refs[0xA8 // 4] == callback["dispose_relocation"]
assert cih["branch_target"](complete, 0x20, 0x419A) == 0x9C  # null bypasses callback

receipt = {
    "head": HEAD, "tree": TREE, "parent": PARENT, "compile_report_sha256": sha(report_path.read_bytes()),
    "compiled": report["passed"], "cih_receipt_sha256": cih_digest, "cih_cases": cases,
    "cih_consumers": consumers, "find_child_ctor_call_offset": hex(get_ctor * 4),
    "compared_comdats": compared, "objects_sha256": {stem: sha(path.read_bytes()) for stem, path in paths.items()},
    "constant_table_callback": {"component_receipt_sha256": callback_digest, "artist_receipt_sha256": artist_digest,
                                "registered_slot": "0x28", "construct_store": "0x204", "callback_body": "blr",
                                "complete_load_restore": "0x80", "dispatch": "0x98", "dispose": "0xa8"},
    "stage": "compile-only",
}
out = ROOT / "R145_COMPOSED_COFF.json"
with out.open("x", encoding="utf-8", newline="\n") as stream:
    json.dump(receipt, stream, indent=2)
    stream.write("\n")
print(out, sha(out.read_bytes()))
