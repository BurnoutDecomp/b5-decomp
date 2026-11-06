import hashlib
import json
from pathlib import Path
import re
import runpy
import subprocess

OUT = Path(__file__).resolve().parent
WORK = Path("D:/Reverse/BP-X360-Experimental-Worktrees/r37-snd9-mixer-lifetime-sol")
ARTIST = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")
R37 = OUT.parent / "r37-link"
BASE = "87e0a50bc1d3e418e7e0617b735c3f0f07350a16"
HEAD = "47c82d4fa591b0c347d32eb7df617a4f2b250176"
tooling = runpy.run_path(str(R37 / "prepare_seal.py"))
parse, relocations = tooling["parse"], tooling["relocations"]


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=WORK)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pin(path):
    path = Path(path)
    return {"path": path.as_posix(), "sha256": sha(path.read_bytes())}


def method(name, listing):
    start = re.search(r"(?m)^" + re.escape(name) + r"\s+PROC\b.*$", listing)
    end = re.search(r"(?m)^" + re.escape(name) + r"\s+ENDP\b", listing)
    assert start and end
    return listing[start.end():end.start()]


def evidence(row, name):
    found = [s for s in records[row] if s[0] == name]
    assert len(found) == 1, (row, name, found)
    return found[0]


assert git("rev-parse", "HEAD").decode().strip() == HEAD
assert git("rev-parse", "HEAD^").decode().strip() == BASE
assert git("diff", "--name-only", BASE, HEAD).count(b"\n") == 8
assert not git("status", "--porcelain")
compile_report = OUT / "native-final/compile-results.json"
rows = json.loads(compile_report.read_text(encoding="utf-8"))["results"]
assert len(rows) == 6 and all(r["passed"] and r["machine"] == 0x1F2 for r in rows)
rows = {Path(r["source"]).stem: r for r in rows}
records = {key: parse(Path(row["object"]).read_bytes()) for key, row in rows.items()}
mixer = "sndmix"
service = "Snd9Service"
allocator = "?gpfnSndAlloc@Snd@@3P6APAXI@ZA"
free = "?gpfnSndFree@Snd@@3P6AXPAX@ZA"
hal_mixer = "?gpMixChannels@Hal@Snd9@@3PAUMixChannel@12@A"
service_rate = "?UpdateSnd9SampleRate@Snd9Service@core@audio@rw@@SAXN@Z"
legacy = {"dword_83277830", "dword_83277834"}
mixer_functions = ("SNDI_New", "SNDI_Delete", "MIX_create", "MIX_destroy", "MIX_createFxglobals",
                   "MIX_destroyFxglobals", "MIXI_initunpackmt", "mixc", "decode16x87")
assert all(evidence(mixer, name)[1] == "comdat" for name in mixer_functions)
assert all(evidence(mixer, name)[1] == "strong" for name in (allocator, free))
assert evidence("sndhal", hal_mixer)[1] == "strong"
assert evidence(mixer, hal_mixer)[1] == "undefined"
assert all(not legacy.intersection(s[0] for s in sy) for sy in records.values())

consumer_relocs = {}
for consumer, targets in {"CMpegBase": {allocator}, "CMpegBase_wO_01": {free},
                          "CMpegLayer3Base": {allocator, free}}.items():
    assert all(evidence(consumer, name)[1] == "undefined" for name in targets)
    consumer_relocs[consumer] = list(relocations(rows[consumer]["object"], targets))
    assert {r["symbol"] for r in consumer_relocs[consumer]} == targets
    assert all(r["target_section"] == 0 for r in consumer_relocs[consumer])

service_targets = {"MIX_create", "MIX_destroy", "byte_83277838", "SNDVOICEI_free"}
service_relocs = list(relocations(rows[service]["object"], service_targets))
service_section = evidence(service, service_rate)[2]
assert all(r["section"] == service_section for r in service_relocs)
assert [r["symbol"] for r in service_relocs if r["type"] == 6] == ["MIX_destroy", "MIX_create"]
assert all(evidence(service, s)[1] == "undefined" for s in service_targets)

filters = {
    "SFILTER_unpackfinit": "0x82B7C8B8",
    "SFILTER_unpacklfinit": "0x82B7C998",
    "SFILTER_unpackpfinit": "0x82B7CB08",
    "SFILTER_unpackxafinit": "0x82B81200",
    "SFILTER_unpackxalfinit": "0x82B81388",
    "SFILTER_unpackxapfinit": "0x82B815B8",
    "SFILTER_unpackmtfinit": "0x82B85500",
    "SFILTER_unpackmtlfinit": "0x82B85688",
    "SFILTER_unpackmtpfinit": "0x82B858C0",
}
assert {s[0] for s in records[mixer] if s[1] == "undefined" and s[0].startswith("SFILTER_")} == set(filters)
mixer_targets = set(filters) | set(mixer_functions) | {allocator, free, hal_mixer, "memcpy"}
mixer_targets.update(s[0] for s in records[mixer] if "@System@core@audio@rw@@" in s[0])
mixer_relocs = list(relocations(rows[mixer]["object"], mixer_targets))
create_section = evidence(mixer, "MIX_create")[2]
destroy_section = evidence(mixer, "MIX_destroy")[2]
create_relocs = [r for r in mixer_relocs if r["section"] == create_section]
destroy_relocs = [r for r in mixer_relocs if r["section"] == destroy_section]
assert {r["symbol"] for r in create_relocs if r["symbol"] in filters} == set(filters)
assert {r["symbol"] for r in create_relocs if r["symbol"] in ("SNDI_New", "SNDI_Delete")} == {"SNDI_New", "SNDI_Delete"}
for relocs, fx_name, mem_method in ((create_relocs, "MIX_createFxglobals", "Alloc"),
                                    (destroy_relocs, "MIX_destroyFxglobals", "Free")):
    calls = [r["symbol"] for r in relocs if r["type"] == 6]
    assert calls[0].startswith("?Lock@System")
    assert calls[1] == fx_name
    assert calls[2].startswith("?" + mem_method + "@System")
    assert calls[-1].startswith("?Unlock@System")

listing = Path(rows[mixer]["assembly"]).read_text(errors="replace")
mix = method("mixc", listing)
pcm = method("decode16x87", listing)
rate = method(service_rate, Path(rows[service]["assembly"]).read_text(errors="replace"))
assert re.search(r"\bfmadds\s+fr\d+,fr\d+,fr1,fr\d+", mix)
assert all(re.search(r"\b" + op + r"\s", pcm) for op in ("lhz", "extsh", "fcfid", "frsp", "stfs"))
assert all(re.search(r"\b" + op + r"\s", rate) for op in ("fadd", "fsub", "frsp", "fctiwz"))
assert re.search(r"\bmulli\s+r\d+,r\d+,92\b", method("MIX_create", listing))

# Reconstruct the table values from the emitted PPC loads and stores.
create = method("MIX_create", listing)
table = create[create.index("0x2C;", create.index("gapfnFilterInit[0]")):]
table = table[table.index(" lis          r11,?gapfnFilterInit"):table.index(" lis          r10,?gpfnDecodeSamples")]
registers = {}
targets = {}
sizes = {}
for line in table.splitlines():
    if match := re.search(r"\baddi\s+(r\d+),r\d+,(SFILTER_\w+)", line):
        registers[match[1]] = match[2]
    elif match := re.search(r"\bli\s+(r\d+),(\d+)", line):
        registers[match[1]] = int(match[2])
    elif match := re.search(r"\bstw\s+(r\d+),([0-9A-F]+)(?:h)?\((r10|r11)\)", line):
        offset = int(match[2], 16)
        (targets if match[3] == "r11" else sizes)[offset // 4] = registers[match[1]]
expected_targets = list(filters)[:6] + ["SFILTER_unpackmtfinit", "SFILTER_unpackmtlfinit",
                                         "SFILTER_unpackmtpfinit"] * 2
expected_sizes = [0x28, 0x2C, 0x34, 0x2C, 0x54, 0x3C, 0x2C, 0x30, 0x3C, 0x2C, 0x30, 0x3C]
assert [targets[i] for i in range(12)] == expected_targets
assert [sizes[i] for i in range(12)] == expected_sizes

diagnostic_path = R37 / "diagnostic-parsed.json"
prior_unresolved = {row["symbol"] for row in json.loads(diagnostic_path.read_text())["unresolved"]}
address_alias_diagnostics = {allocator, free} | legacy
assert {"MIX_create", "MIX_destroy"} | address_alias_diagnostics <= prior_unresolved
assert not set(filters) & prior_unresolved
for name in filters:
    found = subprocess.run(["git", "--no-optional-locks", "grep", "-l", "-w", name, BASE,
                            "--", "src", "vendor"], cwd=WORK, capture_output=True)
    assert found.returncode == 1 and not found.stdout and not found.stderr, (name, found)

patch = OUT / "candidate.patch"
patch.write_bytes(git("diff", "--binary", "--no-ext-diff", BASE, HEAD))
path_rows = {}
for path in git("diff", "--name-only", BASE, HEAD).decode().splitlines():
    new_bytes = git("show", HEAD + ":" + path)
    path_rows[path] = {"new_blob": git("rev-parse", HEAD + ":" + path).decode().strip(),
                       "new_sha256": sha(new_bytes)}
    old = subprocess.run(["git", "cat-file", "-e", BASE + ":" + path], cwd=WORK,
                         capture_output=True)
    if old.returncode == 0:
        path_rows[path]["old_blob"] = git("rev-parse", BASE + ":" + path).decode().strip()
        path_rows[path]["old_sha256"] = sha(git("show", BASE + ":" + path))

original_addresses = ["82B85A40", "82B7BAE8", "82B7BAB0", "82B7BAD0", "82B859B8",
                      "82B7B4D8", "82B7CB98", "82B9BB98", "82B9BC38", "82B85D18"]
source_pins = [pin(ARTIST / ("0x" + address + ".json")) for address in original_addresses
               if (ARTIST / ("0x" + address + ".json")).is_file()]
source_pins.extend(pin(p) for p in sorted(OUT.glob("artist-mixer-*.log")))
result = {
    "status": "PRIVATE_STAGE_NATIVE_COMPILE_AND_COFF_PASS",
    "base": BASE, "head": HEAD, "tree": git("rev-parse", "HEAD^{tree}").decode().strip(),
    "branch": git("branch", "--show-current").decode().strip(),
    "worktree": WORK.as_posix(), "patch": pin(patch), "paths": path_rows,
    "compile_report": pin(compile_report), "native_compiled": len(rows),
    "compiler": pin(rows[mixer]["command"][0]),
    "original_evidence": source_pins,
    "native_products": {row["source"]: {k: pin(row[k]) for k in ("object", "assembly", "log")}
                        for row in rows.values()},
    "mixer_public_providers": list(mixer_functions),
    "allocator_consumer_relocations": consumer_relocs,
    "service_relocations": service_relocs,
    "mixer_create_relocations": create_relocs,
    "mixer_destroy_relocations": destroy_relocs,
    "filter_table_in_emitted_store_order": [
        {"index": i, "target": target, "size": sizes[i]} for i, target in targets.items()],
    "unresolved_filter_owners": filters,
    "prior_link_evidence": pin(diagnostic_path),
    "prior_unresolved_symbols_addressed": sorted({"MIX_create", "MIX_destroy"} | address_alias_diagnostics),
    "prior_unresolved_snd_boundary": ["MIX_audio", "SNDI_aztospkrvol", "SNDPKTPLAYI_flushcallbackdata",
                                     "SNDSYSI_100hzserver", "SNDSYSI_variabletimerservice",
                                     "SNDSYS_restore", "SNDSYS_service", "SNDVOICEI_free", "byte_83277838"],
    "limits": "Targeted PowerPC compile and COFF inspection. Full link and target execution were not run.",
}
receipt = OUT / "RECEIPT.json"
receipt.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
print("patch", sha(patch.read_bytes()))
print("receipt", sha(receipt.read_bytes()))
