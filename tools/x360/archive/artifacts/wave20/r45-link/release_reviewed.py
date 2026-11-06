import hashlib
import json
import pathlib
import subprocess
import sys

here = pathlib.Path(__file__).resolve().parent
wave = here.parent
intake = wave / "r45-intake"
batch = wave / "r45-batch"
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
patch = batch / "r45.patch"
manifest = read(manifest_path)
review = read(review_path)
projection = read(intake / "projection.json")
preparation = read(intake / "PREPARATION_BINDING.json")
assert manifest["base"] == "70aae5c269f26d1e00c950ccc183e4a95c628cce"
assert manifest["expected_total_native"] == projection["native_count"] == 3710
assert review["verdict"].lower() == "pass"
assert review["base"] == manifest["base"]
assert review["projected_tree"] == manifest["projected_tree"]
assert review["manifest_sha256"] == sha(manifest_path)
assert review["patch_sha256"] == sha(patch) == manifest["patch"]["sha256"]
assert preparation["manifest_sha256"] == sha(manifest_path)
assert preparation["projection_sha256"] == sha(intake / "projection.json")

release = {
    "approved": True, "manifest_sha256": sha(manifest_path), "patch_sha256": sha(patch),
    "projection_sha256": sha(intake / "projection.json"), "review_path": str(review_path),
    "review_sha256": sha(review_path), "guard_baseline_sha256": sha(intake / "guard-baseline.json"),
    "netconn_binding_sha256": sha(intake / "netconn-check-binding.json"),
    "entropy_binding_sha256": sha(intake / "entropy-check-binding.json"),
    "loading_binding_sha256": sha(intake / "loading-check-binding.json"),
}
write(intake / "ROOT_RELEASE.json", release)
run(intake / "integrate.py")
run(intake / "run_intake.py")
done = read(intake / "completion.json")
assert done["status"] == "SEALED_NO_REGRESSIONS" and done["passed"] == 3710 and done["failed"] == 0
run(here / "prepare_seal.py")
seal_path = here / "seal.json"
link_release = here / "ROOT_RELEASE.json"
write(link_release, {"action": "one strict R45 native link", "sealed_sha256": sha(seal_path)})
run(here / "run_once.py", "--sealed", str(seal_path), "--sealed-sha256", sha(seal_path),
    "--release", str(link_release))
result = read(here / "result.json")
diagnostics = read(here / "diagnostic-parsed.json")
print("R45 LINK", done["head"], result["exit_code"], diagnostics["unique_symbols"],
      len(diagnostics["duplicate_messages"]), flush=True)

if result["exit_code"] == 0:
    assert result["all_post_checks_pass"] and diagnostics["unique_symbols"] == 0
    assert not diagnostics["duplicate_messages"]
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
    write(here / "image-verification.json", {"head": done["head"], "image": str(xex),
          "image_sha256": sha(xex), "bytes": xex.stat().st_size, "magic": "XEX2",
          "xex_dump_sha256": sha(dump)})
    print("R45 XEX", xex.stat().st_size, sha(xex), flush=True)


