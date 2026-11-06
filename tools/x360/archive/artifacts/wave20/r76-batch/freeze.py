import hashlib
import json
import os
import pathlib
import subprocess


here = pathlib.Path(__file__).resolve().parent
wave = here.parent
work = pathlib.Path("D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra")
base = "5a14195f2976b6cdba809377dd7ea2e2a239ee2c"
head = "a4f793395482369a179fdb9a0d1e6e2bb6324fe8"
projected = "7ce7fdc39237a4a043f8cd518336ed1a3c3ff7db"
paths = [
    "src/GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.cpp",
    "src/GameShared/GameClasses/System/AttribSys/CgsAttribSysVaultResourceType.h",
]


def git(*args, env=None):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=work, env=env)


def pin(path):
    with path.open("rb") as stream:
        return {"path": str(path.resolve()), "sha256": hashlib.file_digest(stream, "sha256").hexdigest()}


def entry(revision, path):
    return git("ls-tree", revision, "--", path).decode().split("\t")[0].strip()


assert git("merge-base", base, head).decode().strip() == base
assert git("rev-parse", head + "^{tree}").decode().strip() == projected
assert git("diff", "--name-only", base, head).decode().splitlines() == paths
git("diff", "--check", base, head)
final = [{"path": path, "old": entry(base, path), "new": entry(head, path)} for path in paths]
assert all(row["old"] and row["new"] and row["old"] != row["new"] for row in final)

patch = here / "r76.patch"
with patch.open("xb") as stream:
    stream.write(git("diff", "--binary", "--full-index", base, head))
index = here / "projection.index"
assert not index.exists()
env = dict(os.environ, GIT_INDEX_FILE=str(index))
git("read-tree", base, env=env)
git("apply", "--cached", "--check", str(patch), env=env)
git("apply", "--cached", str(patch), env=env)
assert git("write-tree", env=env).decode().strip() == projected

result = wave / "r76-vault-registered-preflight/result.json"
receipts = [
    pin(wave.parent / "wave75/attrib-vault-terra/RECEIPT.json"),
    pin(result),
]
assert receipts[0]["sha256"] == "ac2b1170bbab3c1c8edd3fc25a720f921ad2025ec1848448a215305724cebc91"
tested = json.loads(result.read_text(encoding="utf-8"))
assert tested["all_post_checks_pass"] and any(
    row["sha256"] == "b2d9d70800377f3728e37bf7ce8cc06186225151fcf9e28b9680f773ee9ab456"
    for row in tested["outputs"].values()
)
manifest = here / "MANIFEST.json"
data = {
    "schema": "r76-native-intake-v1", "base": base,
    "base_tree": git("rev-parse", base + "^{tree}").decode().strip(),
    "projected_tree": projected, "expected_total_native": 3710, "final_paths": final,
    "groups": [{"name": "native-attribute-vault-fixup", "base": base, "head": head,
                "paths": paths, "patch": pin(patch), "receipts": receipts}],
    "patch": pin(patch), "base_compile": pin(wave / "r74-intake/completion.json"),
    "base_link": pin(wave / "r74-link/result.json"), "base_seal": pin(wave / "r74-link/seal.json"),
    "base_diagnostics": pin(wave / "r74-link/diagnostic-parsed.json"),
    "generated_loading_contract": pin(wave / "r27-loading-terra-relative/NATIVE_INTAKE_INPUT.json"),
    "base_loading_coff": pin(wave / "r74-intake/loading-coff-verification.json"),
    "original_launch_trace": pin(wave / "native-xenia-smoke/run-r74-native-trace/startup-trace-functions.json"),
}
with manifest.open("x", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(pin(manifest), pin(patch), projected)
