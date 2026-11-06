import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
out = Path(__file__).resolve().parent
plan_path = out / "GATE_PLAN.json"
plan = json.loads(plan_path.read_text())
root = Path(plan["source_dir"])
sdk = Path(plan["sdk"])
fallbacks = [Path(path) for path in plan["vendor_fallbacks"]]
driver_path = root / "tools/build_x360.py"
temp = out / "temp"
objects = out / "native"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(name, value):
    with (out / name).open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, indent=2)
        handle.write("\n")


def git(*args):
    return subprocess.check_output(
        ["git", "-c", "core.safecrlf=false", *args], cwd=root, text=True, encoding="utf-8"
    ).strip()


def guards():
    assert git("rev-parse", "HEAD") == plan["head"]
    assert git("branch", "--show-current") == plan["branch"]
    assert sorted(git("diff", "--name-only").splitlines()) == sorted(plan["changed"])
    assert not git("diff", "--cached", "--name-only")
    assert not git("ls-files", "--others", "--exclude-standard")
    for path, expected in {**plan["changed"], **plan["audit_only"]}.items():
        assert sha(root / path) == expected, path
    for path, data in plan["sources"].items():
        assert sha(root / path) == data["sha256"], path
    assert sha(driver_path) == plan["driver_sha256"]
    assert sha(sdk / "bin/win32/cl.exe") == plan["compiler_sha256"]


def snapshot(driver):
    files = set()
    include_roots = [root / p for p in driver.INCLUDES] + [sdk / "include/xbox"] + fallbacks
    for folder in include_roots:
        if folder.is_dir():
            for path in folder.rglob("*"):
                if path.is_file() and path.suffix.lower() in {"", ".h", ".hpp", ".hxx", ".inl", ".inc", ".cpp", ".c"}:
                    files.add(path.resolve())
    files.update((root / p).resolve() for p in plan["sources"])
    files.update(p.resolve() for p in (sdk / "bin/win32").glob("*") if p.name.lower() in
                 {"cl.exe", "c1.dll", "c1xx.dll", "c2.dll", "cl.exe.manifest", "msvcr80.dll", "msvcp80.dll"})
    files.update((Path(__file__).resolve(), driver_path.resolve(), plan_path.resolve()))
    return {str(p).lower(): sha(p) for p in sorted(files)}


def main():
    assert root.drive.upper() == out.drive.upper() == "D:"
    assert plan["jobs"] == 1 and len(fallbacks) == 9 and all(p.is_dir() for p in fallbacks)
    lease = json.loads((out / "LEASE.json").read_text())
    assert lease["approved"] is True and lease["gate_plan_sha256"] == sha(plan_path)
    os.chdir(root)
    guards()
    spec = importlib.util.spec_from_file_location("native_build", driver_path)
    driver = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(driver)
    before = snapshot(driver)
    save("FROZEN_INPUTS_BEFORE.json", before)
    temp.mkdir()
    assert not objects.exists()
    os.environ["TEMP"] = os.environ["TMP"] = str(temp)
    args = [str(driver_path), "--sdk", str(sdk), "--out", str(objects), "--jobs", "1", "--timeout", "180", "--quiet"]
    args.extend("--compiler-arg=/I" + str(path) for path in fallbacks)
    args.append("--compiler-arg=/showIncludes")
    for path in plan["sources"]:
        args.extend(("--source", path))
    save("COMMAND.json", {"args": args, "plan_sha256": sha(plan_path), "source": str(root), "temp": str(temp)})
    print(f"Native Apt gate starting with {len(plan['sources'])} frozen translation units", flush=True)
    sys.argv = args
    code = driver.main()
    guards()
    after = snapshot(driver)
    save("FROZEN_INPUTS_AFTER.json", after)
    changed = [path for path in sorted(before.keys() | after.keys()) if before.get(path) != after.get(path)]
    report = json.loads((objects / "compile-results.json").read_text())
    assert {row["source"].replace("\\", "/") for row in report["results"]} == set(plan["sources"])
    rows = []
    for row in report["results"]:
        found = {}
        log = Path(row["log"])
        for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
            match = re.match(r"\s*Note:\s+including file:\s*(.+?)\s*$", line)
            if match:
                path = Path(match[1]).resolve()
                key = str(path).lower()
                found[str(path)] = {"before": before.get(key), "after": after.get(key),
                                    "stable": key in before and before[key] == after.get(key)}
        artifact_paths = [Path(row[key]) for key in ("object", "assembly", "log") if row.get(key)]
        obj = Path(row["object"]) if row.get("object") else None
        machine = struct.unpack_from("<H", obj.read_bytes())[0] if obj and obj.is_file() else None
        rows.append({**row, "verified_coff": machine, "includes": found,
                     "hashes": {str(p): sha(p) for p in artifact_paths if p.is_file()}})
    passed = [row for row in rows if row["passed"] and row["verified_coff"] == 0x01F2
              and row["includes"] and all(item["stable"] for item in row["includes"].values())]
    result = {"state": "complete", "driver_exit": code, "compiler_jobs": 1, "input_changes": changed,
              "input_count": len(before), "passed": len(passed), "failed": len(rows) - len(passed),
              "rows": rows, "source_head": plan["head"], "gate_plan_sha256": sha(plan_path)}
    save("NATIVE_GATE_RESULT.json", result)
    print(f"Native Apt gate complete: {result['passed']} passed, {result['failed']} failed, {len(changed)} changed inputs",
          flush=True)
    return 0 if code == 0 and not changed and len(passed) == len(rows) else 1


if __name__ == "__main__":
    raise SystemExit(main())
