import hashlib
import json
import pathlib
import subprocess
import re
import sys

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
intake = wave / "r171-intake"
batch = wave / "r171-batch"
review_path = batch / "REVIEW.json"

def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def write(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2)
        stream.write("\n")

def run(path, *args):
    subprocess.run([sys.executable, "-X", "utf8", "-B", str(path), *args], check=True)

manifest_path = batch / "MANIFEST.json"
patch = batch / "r171.patch"
manifest = read(manifest_path)
review = read(review_path)
projection = read(intake / "projection.json")
preparation = read(intake / "PREPARATION_BINDING.json")
assert manifest["base"] == "95836632b166748b4b3ed0a1ed5d57be02122f29"
assert manifest["candidate"] == preparation["candidate"] and manifest["projected_tree"] == preparation["projected_tree"]
assert manifest["expected_total_native"] == projection["native_count"] == 3710
assert review["verdict"].lower() == "pass"
assert review["base"] == manifest["base"] and review["candidate"] == manifest["candidate"]
assert review["projected_tree"] == manifest["projected_tree"]
assert review["manifest_sha256"] == sha(manifest_path)
assert review["patch_sha256"] == sha(patch) == manifest["patch"]["sha256"]
assert preparation["manifest_sha256"] == sha(manifest_path)
assert preparation["projection_sha256"] == sha(intake / "projection.json")
assert preparation["base"] == manifest["base"] and preparation["projected_tree"] == manifest["projected_tree"]
assert preparation["patch_sha256"] == sha(patch)
assert preparation["selected_count"] == len(projection["selected_sources"]) and preparation["retained_count"] == 3710-preparation["selected_count"]
for path, expected in preparation["prepared_files"].items():
    assert sha(pathlib.Path(path)) == expected, path

release = {
    "approved": True, "manifest_sha256": sha(manifest_path), "patch_sha256": sha(patch),
    "projection_sha256": sha(intake / "projection.json"), "review_path": str(review_path),
    "review_sha256": sha(review_path), "guard_baseline_sha256": sha(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r171-guard-baseline/guard-baseline.json')),
    "netconn_binding_sha256": sha(intake / "netconn-check-binding.json"),
    "entropy_binding_sha256": sha(intake / "entropy-check-binding.json"),
    "loading_binding_sha256": sha(intake / "loading-check-binding.json"),
}
write(intake / "ROOT_RELEASE.json", release)
run(intake / "integrate.py")
run(intake / "run_intake.py")
done = read(intake / "completion.json")
assert done["status"] == "SEALED_NO_REGRESSIONS" and done["passed"] == 3710 and done["failed"] == 0
assert (done["fresh"], done["retained"]) == (preparation["selected_count"], preparation["retained_count"])
guards = read(intake / "named-guards.json")
assert len(guards) == 7 and all(row["gate_satisfied"] for row in guards)
assert {row["name"] for row in guards if row["kind"] == "fresh"} == {row["name"] for row in projection["named_guard_impacts"] if row["changed_source"] or row["changed_actual_headers"]}

run(here / "prepare_seal_v4.py")
seal_path = here / "seal.json"
link_release = here / "ROOT_RELEASE.json"
write(link_release, {"action": "one strict R171 native link", "sealed_sha256": sha(seal_path)})
run(here / "run_once_v4.py", "--sealed", str(seal_path), "--sealed-sha256", sha(seal_path),
    "--release", str(link_release))
result = read(here / "result.json")
diagnostics = read(here / "diagnostic-parsed.json")
print("R171 LINK", done["head"], result["exit_code"], diagnostics["unique_symbols"],
      len(diagnostics["duplicate_messages"]), flush=True)

if result["exit_code"] == 0:
    assert result["all_post_checks_pass"] and diagnostics["unique_symbols"] == 0
    assert not diagnostics["duplicate_messages"]
    run(here / "verify_xcontent_provider.py")
    xex = here / "native-game.exe"
    with xex.open("rb") as stream:
        assert stream.read(4) == b"XEX2"
    assert sha(xex) == result["outputs"][str(xex)]["sha256"]
    sdk_imagexex = pathlib.Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK/bin/win32/imagexex.exe")
    dump = here / "xex-dump.log"
    with dump.open("xb") as output:
        subprocess.run([str(sdk_imagexex), "/DUMP", "/IN:" + str(xex)], cwd=here,
                       stdout=output, stderr=subprocess.STDOUT, check=True)
    text = dump.read_text(errors="replace")
    assert "entry point" in text and "xboxkrnl.exe" in text and "XHV 2.0.6534.1" in text
    assert "NOT-COMPRESSED, NOT-ENCRYPTED" in text and re.search(r"(?im)^\s*45410806\s+title ID\s*$", text)
    mapped_verifier = pathlib.Path("D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r161-execution-id-preflight/verify_native_title_xex.py")
    assert sha(mapped_verifier) == "b759f3cb40476040cccbbe43561d9e0e2d232230c7e5f294715687e1e4ebcb6b"
    mapped_receipt = here / "mapped-image-verification.json"
    run(mapped_verifier, "--pe", str(here / "native-game.pe"), "--map", str(here / "native-game.map"), "--xex", str(xex), "--output", str(mapped_receipt))
    mapped = read(mapped_receipt)
    assert mapped["status"] == "pass" and mapped["title_id"] == "0x45410806" and mapped["different_bytes"]["outside_import_ranges"] == 0
    assert mapped["pe_sha256"] == sha(here / "native-game.pe") and mapped["map_sha256"] == sha(here / "native-game.map") and mapped["xex_sha256"] == sha(xex)
    write(here / "image-verification.json", {"head": done["head"], "image": str(xex),
          "image_sha256": sha(xex), "bytes": xex.stat().st_size, "magic": "XEX2",
          "xex_dump_sha256": sha(dump), "mapped_image_verification_sha256": sha(mapped_receipt), "mapped_verifier_sha256": sha(mapped_verifier)})
    print("R171 XEX", xex.stat().st_size, sha(xex), flush=True)


