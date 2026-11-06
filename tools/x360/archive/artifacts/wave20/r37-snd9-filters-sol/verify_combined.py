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
PARENT = "2759f5f141c902fc944684dfc56904b732b622ae"
HEAD = "8038196e8d38d82af94fecdd64cdb0c0ca8d357e"
tools = runpy.run_path(str(R37 / "prepare_seal.py"))
parse, archive, relocations = (tools[name] for name in ("parse", "archive", "relocations"))


def git(*args):
    return subprocess.check_output(["git", "--no-optional-locks", *args], cwd=WORK)


def pin(path):
    path = Path(path)
    return {"path": path.as_posix(), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}


def read(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


assert git("rev-parse", HEAD + "^").decode().strip() == PARENT
assert git("merge-base", BASE, HEAD).decode().strip() == BASE
assert not git("diff", "--check", PARENT, HEAD)
combined_report = OUT / "native-combined/compile-results.json"
rows = read(combined_report)["results"]
assert len(rows) == 11 and all(row["passed"] and row["machine"] == 0x1F2 for row in rows)
rows = {Path(row["source"]).stem: row for row in rows}
symbols = {key: parse(Path(row["object"]).read_bytes()) for key, row in rows.items()}
definitions = {}
for source, sym in symbols.items():
    for name, kind, *_ in sym:
        if kind in ("strong", "comdat"):
            definitions.setdefault(name, set()).add(source)

filters = {
    "SFILTER_unpackfinit": "82B7C8B8",
    "SFILTER_unpacklfinit": "82B7C998",
    "SFILTER_unpackpfinit": "82B7CB08",
    "SFILTER_unpackxafinit": "82B81200",
    "SFILTER_unpackxalfinit": "82B81388",
    "SFILTER_unpackxapfinit": "82B815B8",
    "SFILTER_unpackmtfinit": "82B85500",
    "SFILTER_unpackmtlfinit": "82B85688",
    "SFILTER_unpackmtpfinit": "82B858C0",
}
assert all(definitions[name] == {"sndfilter"} for name in filters)
mixer_filters = list(relocations(rows["sndmix"]["object"], set(filters)))
assert {r["symbol"] for r in mixer_filters} == set(filters)
assert all(r["target_section"] == 0 and r["type"] in (16, 17) for r in mixer_filters)

packet_names = {"SNDPKTPLAY_create", "SNDPKTPLAY_submit", "SNDPKTPLAYI_flushcallbackdata",
                "SNDPKTPLAYI_freeframes", "SNDPKTPLAYI_get", "SNDPKTPLAYI_voicetopackethandle"}
packet_names.add(next(n for n in definitions if n.startswith("?FindVoiceHandle@Packet@Snd9@@")))
assert all(definitions[name] == {"sndpacket"} for name in packet_names)
filter_packet_edges = list(relocations(rows["sndfilter"]["object"], packet_names | {"SNDDRV_getsamplechan"}))
expected_filter_edges = {"SNDPKTPLAYI_freeframes", "SNDPKTPLAYI_get", "SNDDRV_getsamplechan"} | {
    n for n in packet_names if n.startswith("?FindVoiceHandle@")}
assert {r["symbol"] for r in filter_packet_edges} == expected_filter_edges
assert all(r["type"] == 6 for r in filter_packet_edges)
packet_voice_edges = list(relocations(rows["sndpacket"]["object"], {"SNDVOICEI_get"}))
assert packet_voice_edges and all(r["type"] == 6 for r in packet_voice_edges)
assert definitions["SNDVOICEI_get"] == {"sndsys"}
assert definitions["SNDDRV_getsamplechan"] == {"sndsys"}

old_names = {"byte_83277838", "dword_83277830", "dword_83277834"}
assert not old_names & {n for sym in symbols.values() for n, *_ in sym}
alias_alloc = "?gpfnSndAlloc@Snd@@3P6APAXI@ZA"
alias_free = "?gpfnSndFree@Snd@@3P6AXPAX@ZA"
assert all(definitions[name] == {"sndmix"} for name in (alias_alloc, alias_free))
for consumer, expected in (("CMpegBase", {alias_alloc}), ("CMpegBase_wO_01", {alias_free}),
                            ("CMpegLayer3Base", {alias_alloc, alias_free})):
    edges = list(relocations(rows[consumer]["object"], expected))
    assert {r["symbol"] for r in edges} == expected
service_targets = {"MIX_create", "MIX_destroy", "MIX_audio", "SNDPKTPLAYI_flushcallbackdata",
                   "SNDSYS_service", "SNDSYSI_100hzserver", "SNDSYSI_variabletimerservice",
                   "?gOptions@Snd9@@3USystemOptions@1@A"}
service_edges = list(relocations(rows["Snd9Service"]["object"], service_targets))
assert {r["symbol"] for r in service_edges} == service_targets
assert [r["symbol"] for r in service_edges if r["symbol"] in
        {"MIX_audio", "SNDPKTPLAYI_flushcallbackdata"}] == ["MIX_audio", "SNDPKTPLAYI_flushcallbackdata"]

asm = {key: Path(row["assembly"]).read_text(errors="replace") for key, row in rows.items()}
assert re.search(r"\bsth\s+r\d+,2Ch\(r\d+\)", asm["sndfilter"])
assert re.search(r"\blhz\s+r\d+,2Ch\(r\d+\)", asm["sndfilter"])
assert re.search(r"\bmulli\s+r\d+,r\d+,132\b", asm["sndsys"])
assert re.search(r"\bsrawi\s+r\d+,r\d+,1\b", asm["sndpacket"])
assert re.search(r"\bfmadds\s+fr\d+,fr\d+,fr1,fr\d+", asm["sndmix"])
assert re.search(r"\bfctiwz\s+fr\d+,fr\d+", asm["Snd9Service"])

undefined = {n for sym in symbols.values() for n,k,*_ in sym if k == "undefined"} - definitions.keys()
prior_diagnostics = R37 / "diagnostic-parsed.json"
prior = {r["symbol"] for r in read(prior_diagnostics)["unresolved"]}
fixed = {"MIX_create", "MIX_destroy", "SNDPKTPLAYI_flushcallbackdata", alias_alloc, alias_free} | old_names
assert fixed <= prior and not fixed & undefined
expected_remaining = {"MIX_audio", "SNDI_aztospkrvol", "SNDSYSI_100hzserver",
                      "SNDSYSI_variabletimerservice", "SNDSYS_restore", "SNDSYS_service", "SNDVOICEI_free"}
assert undefined & prior == expected_remaining

other = undefined - prior
base_providers = {}
seal_path = R37 / "seal.json"
seal = read(seal_path)
for row in seal["selected"]:
    if row["source"] in {f"vendor/renderware/src/rw/audio/core/{n}.cpp"
                         for n in ("System", "TimerManager", "TimerHandle")}:
        item = Path(row["object"])
        item_pin = pin(item)
        assert item_pin["sha256"] == row["sha256"]
        for name, kind, *_ in parse(item.read_bytes()):
            if name in other and kind in ("strong", "comdat"):
                base_providers[name] = item_pin
for lib in seal["libraries"]:
    if Path(lib["path"]).name.lower() not in ("libcmt.lib", "libcpmt.lib"):
        continue
    item = Path(lib["path"])
    item_pin = pin(item)
    assert item_pin["sha256"] == lib["sha256"]
    for member, data in archive(item):
        for name, kind, *_ in parse(data) or ():
            if name in other and kind in ("strong", "comdat", "common", "absolute"):
                base_providers[name] = item_pin | {"member": member}
assert not other - base_providers.keys(), sorted(other - base_providers.keys())

def write_patch(name, base):
    patch = OUT / name
    patch.write_bytes(git("diff", "--binary", "--no-ext-diff", base, HEAD))
    return pin(patch)

files = {}
for path in git("diff", "--name-only", BASE, HEAD).decode().splitlines():
    files[path] = {
        "blob": git("rev-parse", HEAD + ":" + path).decode().strip(),
        "sha256": hashlib.sha256(git("show", HEAD + ":" + path)).hexdigest(),
    }
assert len(files) == 16, len(files)
source_addresses = set(filters.values()) | {
    "82B72FA8", "82B733E0", "82B73F90", "82B74068", "82B74190", "82B74388", "82B743F8",
    "82B74780", "82B74848", "82B7E290", "82B7E410", "82B7D2E0", "82B7C9F8", "82B814A0",
    "82B9BC38", "82B85A40", "82B7BAE8",
}
record = {
    "status": "PRIVATE_MIXER_FILTER_BOUNDARY_NATIVE_COMPILE_AND_COFF_PASS",
    "base": BASE, "previous_stage": PARENT, "head": HEAD,
    "tree": git("rev-parse", HEAD + "^{tree}").decode().strip(),
    "stage_patch": write_patch("stage-3.patch", PARENT),
    "combined_patch": write_patch("combined-r37-mixer-and-filters.patch", BASE),
    "files": files, "combined_compile": pin(combined_report), "native_compiled": len(rows),
    "products": {r["source"]: {k: pin(r[k]) for k in ("object", "assembly", "log")} for r in rows.values()},
    "original_evidence": [pin(ARTIST / ("0x" + address + ".json")) for address in sorted(source_addresses)
                          if (ARTIST / ("0x" + address + ".json")).is_file()] +
                         [pin(OUT / "artist-filter-closure.json"), pin(OUT / "artist-packet-data.json")],
    "filter_providers": filters, "mixer_filter_relocations": mixer_filters,
    "filter_packet_relocations": filter_packet_edges, "service_relocations": service_edges,
    "existing_external_provider_proof": base_providers,
    "prior_link_report": pin(prior_diagnostics), "prior_link_seal": pin(seal_path),
    "prior_unresolved_symbols_addressed": sorted(fixed),
    "prior_unresolved_symbols_in_combined_objects": sorted(expected_remaining),
    "new_unresolved_symbols_in_combined_objects": [],
    "additional_runtime_owners_outside_mixer_filter_boundary": [
        "SNDPKTPLAY_start", "SNDPKTPLAY_stop", "SNDPKTPLAY_destroy",
        "SNDSYSI_init", "SNDPLATFORM_packetplay", "MIX_playinit", "MIX_audioslice"],
    "limits": "Targeted PowerPC compile and COFF inspection. Full link and live audio runtime were not run.",
}
receipt = OUT / "STAGE_3_COMBINED_RECEIPT.json"
receipt.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8", newline="\n")
print(pin(receipt))
print(record["stage_patch"])
print(record["combined_patch"])
