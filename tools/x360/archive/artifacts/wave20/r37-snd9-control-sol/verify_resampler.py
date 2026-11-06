import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

OUT = Path(__file__).resolve().parent
WORK = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r37-snd9-mixer-lifetime-sol")
R37 = OUT.parent / "r37-link"
BASE = "b58e54f1a87a4dc79a33aec9d0e018b42cdeb3ed"
HEAD = "cae51e3efb8b06b0b810d6d9aa185f8c55feca41"
RENDERER = "9b35bb11498ae440fdf4df0c38b5c5eeb0d40ff0"
SERVICE = "bd52f8b781a28851b365c55cc7d10fbe5dfd4ae7"
tools = runpy.run_path(str(R37 / "prepare_seal.py"))
parse, relocations, archive = (tools[name] for name in ("parse", "relocations", "archive"))


def pin(path):
    path = Path(path)
    return {"path": path.as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=WORK)


def method(name, listing):
    match = re.search(r"(?ms)^([^;\s]*" + name + r"[^\s;]*)\s+PROC.*?^\1\s+ENDP", listing)
    assert match, name
    return match[0]


assert git("rev-parse", HEAD + "^").decode().strip() == BASE
assert not git("-c", "core.whitespace=cr-at-eol", "diff", "--check", BASE, HEAD)
expected_files = {"sndresample.cpp", "sndcontrol.cpp", "sndhal.h", "sndmix.h"}
changed = git("diff", "--name-only", BASE, HEAD).decode().splitlines()
assert {Path(path).name for path in changed} == expected_files
report_path = OUT / "native-resample/compile-results.json"
rows = read(report_path)["results"]
assert len(rows) == 9 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
rows = {Path(row["source"]).stem: row for row in rows}
objects = {key: Path(row["object"]).read_bytes() for key, row in rows.items()}
symbols = {key: parse(obj) for key, obj in objects.items()}
definitions = {}
for owner, entries in symbols.items():
    for name, kind, section, *_ in entries:
        if kind in ("strong", "comdat"):
            definitions.setdefault(name, []).append((owner, section))

assert definitions["MIX_setpitch"][0][0] == "sndresample" and len(definitions["MIX_setpitch"]) == 1
assert definitions["MIX_stop"][0][0] == "sndmix_audio" and len(definitions["MIX_stop"]) == 1
assert definitions["SFILTER_add"][0][0] == "sndresample" and len(definitions["SFILTER_add"]) == 1
control_refs = list(relocations(rows["sndcontrol"]["object"], {"MIX_stop", "MIX_setpitch", "SNDVOICEI_free"}))
assert [r["symbol"] for r in control_refs if r["type"] == 6] == ["MIX_setpitch", "MIX_stop", "SNDVOICEI_free"]
alloc = "?Alloc@System@core@audio@rw@@SAPAXPAV1234@IPBDIPAVICoreAllocator@Allocator@EA@@@Z"
create_refs = list(relocations(rows["sndresample"]["object"], {alloc, "SFILTER_add"}))
pitch_section = definitions["MIX_setpitch"][0][1]
assert [r["symbol"] for r in create_refs if r["type"] == 6 and r["section"] == pitch_section] == [alloc, "SFILTER_add"]

control_missing = {name for key in ("sndcontrol", "sndresample") for name,kind,*_ in symbols[key]
                   if kind == "undefined" and name not in definitions}
seal_path = R37 / "seal.json"
seal = read(seal_path)
base_providers = {}
system = next(row for row in seal["selected"] if row["source"] == "vendor/renderware/src/rw/audio/core/System.cpp")
system_pin = pin(system["object"])
assert system_pin["sha256"] == system["sha256"]
for name, kind, *_ in parse(Path(system["object"]).read_bytes()):
    if kind in ("strong", "comdat") and name in control_missing:
        base_providers[name] = system_pin
for lib in seal["libraries"]:
    if Path(lib["path"]).name.lower() != "libcmt.lib":
        continue
    library_pin = pin(lib["path"])
    assert library_pin["sha256"] == lib["sha256"]
    for member, data in archive(Path(lib["path"])):
        for name, kind, *_ in parse(data) or ():
            if name in control_missing and kind in ("strong", "comdat", "common", "absolute"):
                base_providers[name] = library_pin | {"member": member}
assert not control_missing - base_providers.keys(), sorted(control_missing - base_providers.keys())

listing = Path(rows["sndresample"]["assembly"]).read_text(errors="replace")
linear = method("ResampleLinear", listing)
cubic = method("ResampleCubic", listing)
process = method("ProcessResample", listing)
pitch = method("MIX_setpitch", listing)
assert all(re.search(r"\b" + op + r"\s", linear) for op in ("fsubs", "fmuls", "fmadds"))
assert all(re.search(r"\b" + op + r"\s", cubic) for op in ("fsubs", "fmsubs", "fnmsubs", "fmuls", "fmadds", "fadds"))
assert re.search(r"\bsth\s+r\d+,20h\(r\d+\)", process)
assert len(re.findall(r"\bbctrl\b", process)) == 3
assert re.search(r"\bmulli\s+r\d+,r\d+,92\b", pitch)
assert re.search(r"\bstw\s+r\d+,50h\(r\d+\)", pitch)
assert re.search(r"\bstw\s+r\d+,1Ch\(r\d+\)", pitch)
data_path = OUT / "artist-resample-data.json"
data = read(data_path)
for address in ("0x8200426c", "0x82001d9c", "0x82001da0", "0x82004270", "0x821565b0"):
    assert bytes.fromhex(data[address]["data"]) in objects["sndresample"]
assert not git("diff", RENDERER, BASE, "--", "src/SDKs/EATech/include/snd/sndmix_audio.cpp")

patch = OUT / "stage-2-resample.patch"
patch.write_bytes(git("diff", "--binary", "--no-ext-diff", BASE, HEAD))
files = {path: {"blob": git("rev-parse", HEAD + ":" + path).decode().strip(),
                "sha256": hashlib.sha256(git("show", HEAD + ":" + path)).hexdigest()}
         for path in changed}
record = {
    "status": "PRIVATE_CONTROL_AND_RESAMPLE_NATIVE_COMPILE_AND_COFF_PASS",
    "base_private_composition": BASE, "head": HEAD,
    "tree": git("rev-parse", HEAD + "^{tree}").decode().strip(),
    "exact_renderer_source": RENDERER, "exact_service_source": SERVICE,
    "stage_patch": pin(patch), "files": files,
    "compile_report": pin(report_path), "native_compiled": len(rows),
    "products": {r["source"]: {k: pin(r[k]) for k in ("object", "assembly", "log")} for r in rows.values()},
    "control_provider_patch": pin(OUT / "STAGE_1_CONTROL_RECEIPT.json"),
    "artist_function_evidence": pin(OUT / "artist-snd-control.json"),
    "artist_data_evidence": pin(data_path),
    "control_call_relocations": control_refs, "pitch_install_relocations": create_refs,
    "resolved_coff_providers": {name: definitions[name] for name in ("MIX_stop", "MIX_setpitch", "SFILTER_add",
                                  "SNDVOICEI_free", "iSNDcalcpitch", "iSNDcalcvol", "SNDPLATFORM_setpitch", "SNDstop")},
    "base_provider_evidence": base_providers,
    "unresolved_symbols_in_control_and_resampler_after_provider_accounting": [],
    "limits": "Targeted PowerPC compile and COFF inspection. Full link and live audio were not run.",
}
receipt = OUT / "STAGE_2_RESAMPLE_RECEIPT.json"
receipt.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8", newline="\n")
print(pin(receipt))
print(pin(patch))
