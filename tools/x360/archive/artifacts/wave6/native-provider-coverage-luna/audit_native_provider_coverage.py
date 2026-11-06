#!/usr/bin/env python3
"""Read-only provider coverage audit for the immutable d3fc XDK compile snapshot."""
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

OUT = Path(__file__).resolve().parent
HEAD = "d3fc771a05e4f1b3da4c10c53da557cd195d54f7"
SOURCE_REPO = Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
CANONICAL = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
REPORT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave6\repair-stage1\compile-results.json")
AUDIT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave6\repair-stage1-terminal-audit.json")
PATCH_META = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave6\native-parallel-wave6-final-patch.json")
OLD_INV = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave6\link-inventory-refresh-luna\native-link-inventory.json")
SDK = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK")

def sha(data):
    return hashlib.sha256(data).hexdigest()

def sha_file(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def git(repo, *args):
    return subprocess.check_output(["git", "-C", str(repo), *args], stdin=subprocess.DEVNULL)

def git_blob(path):
    return git(SOURCE_REPO, "show", f"{HEAD}:{path}")

def parse_coff(blob):
    if len(blob) < 20:
        raise ValueError("truncated COFF object")
    machine, nsec, _, ptrsym, nsym, opt, _ = struct.unpack_from("<HHIIIHH", blob)
    if machine != 0x01F2 or opt != 0:
        raise ValueError(f"unexpected native object format: machine={machine:#x}, opt={opt}")
    strtab = ptrsym + nsym * 18 if ptrsym else 0
    strsize = struct.unpack_from("<I", blob, strtab)[0] if ptrsym and strtab + 4 <= len(blob) else 0

    def symbol_name(raw):
        if raw[:4] == b"\0\0\0\0":
            offset = struct.unpack_from("<I", raw, 4)[0]
            if 4 <= offset < strsize:
                end = blob.find(b"\0", strtab + offset)
                return blob[strtab + offset:end].decode("utf-8", "replace")
        return raw[:8].split(b"\0", 1)[0].decode("latin1")

    sections = []
    base = 20 + opt
    for i in range(nsec):
        off = base + 40 * i
        name = blob[off:off + 8].split(b"\0", 1)[0].decode("latin1")
        size, rawptr = struct.unpack_from("<II", blob, off + 16)
        sections.append((name, size, rawptr))
    symbols, i = [], 0
    while i < nsym:
        off = ptrsym + 18 * i
        raw = blob[off:off + 18]
        name = symbol_name(raw)
        value = struct.unpack_from("<I", raw, 8)[0]
        section = struct.unpack_from("<h", raw, 12)[0]
        storage, aux = raw[16], raw[17]
        if storage == 2:
            symbols.append((name, value, section))
        i += 1 + aux
    directives = b"\n".join(blob[p:p + size] for name, size, p in sections if name == ".drectve")
    defaults = [m.decode("ascii", "replace").upper()
                for m in re.findall(rb"/DEFAULTLIB\s*:\s*\"?([^\"\x00\s]+)", directives, re.I)]
    return symbols, defaults

def source_family(path):
    for prefix, name in (
        ("vendor/EAThread/", "EAThread"), ("vendor/EABase/", "EABase"),
        ("vendor/EASTL/", "EASTL"), ("vendor/PPMalloc/", "PPMalloc"),
        ("vendor/coreallocator/", "CoreAllocator"), ("vendor/dirtysdk/", "DirtySDK"),
        ("vendor/zlib/", "zlib"), ("vendor/lua/", "Lua"),
        ("vendor/renderware/", "RenderWare")):
        if path.startswith(prefix):
            return name
    return "engine"

def name_family(name):
    low = name.lower()
    if low.startswith("lua"):
        return "Lua"
    if low.startswith(("inflate", "deflate", "crc32", "adler32", "compress", "uncompress", "gz")):
        return "zlib"
    if "eastl@@" in low or "eastl::" in low:
        return "EASTL"
    if re.match(r"^(?:\?\?[01])?(?:[^@]+@)?(?:thread|semaphore|mutex|rwmutex|condition|barrier|futex|threadlocalstorage)@thread@ea@@", low):
        return "EAThread"
    if re.match(r"^(?:\?\?[01])?(?:[^@]+@)?(?:icoreallocator|generalallocator)@allocator@ea@@", low):
        return "PPMalloc/CoreAllocator"
    if low.startswith(("dirty", "netconn", "netlib", "netgame", "socket", "prot", "lobby", "crypt", "voip", "pingmanager", "gamemanager", "telemetry")):
        return "DirtySDK"
    if "@rw@@" in low or "@rp@@" in low or "rw::" in low or "rp::" in low:
        return "RenderWare"
    return None

# Bind the report to the immutable terminal artifact receipt before touching objects.
report_sha, audit_sha, patch_meta_sha = sha_file(REPORT), sha_file(AUDIT), sha_file(PATCH_META)
audit = json.loads(AUDIT.read_text(encoding="utf-8"))
report = json.loads(REPORT.read_text(encoding="utf-8"))
patch_meta = json.loads(PATCH_META.read_text(encoding="utf-8"))
assert audit["head"] == HEAD and audit["report_sha256"] == report_sha
assert audit["metadata_sha256"] == patch_meta_sha
assert patch_meta["head"] == HEAD and patch_meta["sha256"] == audit["patch_sha256"]
assert report["state"] == "complete" and report["requested"] == 3613
assert report["passed"] == 3374 and report["failed"] == 239
rows = {r["source"]: r for r in report["results"]}
assert len(rows) == report["requested"]
bound_objects = audit["artifact_manifest"]
assert set(bound_objects) == set(rows)

# Parse current successful PPC/BE objects only. Failing engine sources remain candidates,
# but have no object and are not used to infer the eventual link's complete undefined set.
definitions = defaultdict(list)
undefined = defaultdict(list)
undefined_by_family = Counter()
definitions_by_source_family = Counter()
defaults = Counter()
object_hashes = {}
objects_scanned = 0
for source, row in rows.items():
    if not row["passed"]:
        continue
    path = Path(row["object"])
    digest = sha_file(path)
    expected = bound_objects[source]["object_sha256"]
    assert digest == expected, (source, digest, expected)
    blob = path.read_bytes()
    symbols, libs = parse_coff(blob)
    object_hashes[source] = digest
    defaults.update(libs)
    fam = source_family(source)
    for name, value, section in symbols:
        if section > 0 or (section == 0 and value > 0) or section == -1:
            definitions[name].append({"source": source, "object_sha256": digest, "family": fam})
            definitions_by_source_family[fam] += 1
        elif section == 0 and value == 0:
            undefined[name].append(source)
            sf = name_family(name)
            if sf:
                undefined_by_family[sf] += 1
    objects_scanned += 1
assert objects_scanned == report["passed"]

unresolved_names = {name: refs for name, refs in undefined.items() if name not in definitions}
known_crt = json.loads(OLD_INV.read_text(encoding="utf-8"))["summary"]["crt_and_link_support"]["xdk_archive_symbol_evidence"]
libdir = SDK / "lib/xbox"
defaultlib_archive_map = {"OLDNAMES": "oldnames.lib", "LIBCMT": "libcMT.lib",
                          "XAPILIB": "xapilib.lib", "XBOXKRNL": "xboxkrnl.lib",
                          "LIBCPMT": "libcpMT.lib"}
archive_evidence = {}
for directive, archive in defaultlib_archive_map.items():
    path = libdir / archive
    assert path.is_file(), str(path)
    digest = sha_file(path)
    prior = next((v for k, v in known_crt.items() if k.lower() == archive.lower()), None)
    if prior:
        assert prior["sha256"] == digest, (archive, prior["sha256"], digest)
        provenance = "prior symbol evidence reused after archive SHA-256 equality; no rescan"
        symbols = prior.get("symbols", {})
    else:
        provenance = "archive present and hashed; symbol contents not scanned"
        symbols = None
    archive_evidence[directive] = {"archive": archive, "path": str(path), "sha256": digest,
                                   "objects_requesting_defaultlib": defaults[directive],
                                   "prior_symbol_evidence": symbols, "provenance": provenance}

# Read only the pinned d3fc parent Git tree for CMake/source facts.
tree_raw = git(SOURCE_REPO, "ls-tree", "-r", "-z", HEAD)
tracked = []
tracked_blob_oids = {}
gitlinks = {}
for rec in tree_raw.split(b"\0"):
    if not rec:
        continue
    meta, rawpath = rec.split(b"\t", 1)
    mode, typ, oid = meta.decode("ascii").split(" ", 2)
    path = rawpath.decode("utf-8")
    if mode == "160000":
        gitlinks[path] = oid
    elif path.lower().endswith((".c", ".cpp")):
        tracked.append(path)
        tracked_blob_oids[path] = oid
assert len(tracked) == 4354

def count_paths(prefix):
    return sum(1 for p in tracked if p.startswith(prefix))

def cmake_blob(path):
    raw = git_blob(path)
    return {"path": path, "git_blob_oid": git(SOURCE_REPO, "rev-parse", f"{HEAD}:{path}").decode().strip(),
            "sha256": sha(raw), "text": raw.decode("utf-8", "replace")}

cmake_paths = ["CMakeLists.txt", "src/CMakeLists.txt", "vendor/CMakeLists.txt",
               "vendor/PPMalloc/CMakeLists.txt", "vendor/coreallocator/CMakeLists.txt",
               "vendor/dirtysdk/CMakeLists.txt", "vendor/dirtysdk/src/CMakeLists.txt",
               "vendor/zlib/CMakeLists.txt", "vendor/renderware/CMakeLists.txt"]
cmake = {p: cmake_blob(p) for p in cmake_paths}
for p in cmake_paths:
    if not cmake[p]["text"]:
        raise RuntimeError("missing pinned CMake file: " + p)

search = subprocess.run(["git", "-C", str(SOURCE_REPO), "grep", "-n", "-i", "-E",
                         "-e", r"EAGeneralAllocator[.]cpp|vendor/lua|lua/src|vendor/renderware/src|BaseResourceDescriptor[.]cpp|SDKs/EATech/eathread|EAThread|PPMalloc|CoreAllocator|DirtySDK|zlib",
                         HEAD, "--", "CMakeLists.txt", "**/CMakeLists.txt"],
                        stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
if search.returncode not in (0, 1):
    raise RuntimeError(search.stderr.decode("utf-8", "replace"))
cmake_search = search.stdout.decode("utf-8", "replace").splitlines()
tool_paths = ["tools/build_x360.py", "tools/prepare_x360_linker.py", ".gitmodules"]
tool_evidence = {p: {"git_blob_oid": git(SOURCE_REPO, "rev-parse", f"{HEAD}:{p}").decode().strip(),
                     "sha256": sha(git_blob(p)),
                     "text_excerpt": git_blob(p).decode("utf-8", "replace")[:2400]}
                 for p in tool_paths}

module_specs = {"EABase": {"target": "EABase", "kind": "header-only INTERFACE", "source_glob": None},
                "EASTL": {"target": "EASTL", "kind": "compiled static library", "source_glob": "source/*.cpp"},
                "EAThread": {"target": "EAThread", "kind": "compiled static library", "source_glob": "source/*.cpp"}}
modules = {}
for name, spec in module_specs.items():
    parent_path = "vendor/" + name
    pin = gitlinks[parent_path]
    repo = CANONICAL / "vendor" / name
    actual = git(repo, "rev-parse", "HEAD").decode().strip()
    assert actual == pin
    cmake_raw = git(repo, "show", f"{pin}:CMakeLists.txt")
    cmake_text = cmake_raw.decode("utf-8", "replace")
    if spec["source_glob"]:
        sub_tree = git(repo, "ls-tree", "-r", "--name-only", pin).decode().splitlines()
        expected_sources = ["vendor/" + name + "/" + p for p in sub_tree
                            if p.startswith("source/") and p.endswith(".cpp") and "/" not in p[len("source/"):]]
    else:
        expected_sources = []
    modules[name] = {**spec, "gitlink_commit": pin, "cmake_sha256": sha(cmake_raw),
                     "source_cpp_count_in_submodule": len(expected_sources),
                     "source_cpp_paths": expected_sources,
                     "xdk_native_compile_report_rows": sum(1 for p in expected_sources if p in rows),
                     "xdk_native_objects_emitted_for_glob_sources": sum(1 for p in expected_sources if p in object_hashes),
                     "source_tree_has_xbox_or_xenon_paths": any(re.search(r"(^|/)(xbox|xenon|xenon360|ppc)(/|$)", p, re.I) for p in git(repo, "ls-tree", "-r", "--name-only", pin).decode().splitlines()),
                     "cmake_excerpt": [line.strip() for line in cmake_text.splitlines()
                                       if any(t in line for t in ("file(GLOB", "add_library(", "INTERFACE", "target_sources(", "target_link_libraries("))]}

report_paths = set(rows)
cmake_sources = {
    "PPMalloc": [],
    "CoreAllocator": ["vendor/coreallocator/source/icoreallocator_interface.cpp"],
    "DirtySDK": ["vendor/dirtysdk/src/" + x for x in (
        "dirtyaddr.cpp", "dirtylib.cpp", "dirtymem.cpp", "dirtynet.cpp", "lobbyapiutil.cpp",
        "lobbydisplist.cpp", "lobbyname.cpp", "lobbysort.cpp", "lobbytagfield.cpp",
        "plat-str.cpp", "plat-time.cpp", "protomangle.cpp")],
    "zlib": ["vendor/zlib/src/" + x for x in (
        "adler32.c", "crc32.c", "inffast.c", "inflate.c", "inftrees.c", "uncompr.c", "zutil.c")],
    "Lua": [],
    "RenderWare": [],
}

families = {}
for name in ("EABase", "EASTL", "EAThread", "PPMalloc", "CoreAllocator", "DirtySDK", "zlib", "Lua", "RenderWare"):
    prefix = "vendor/" + {"CoreAllocator": "coreallocator", "DirtySDK": "dirtysdk", "RenderWare": "renderware"}.get(name, name) + "/"
    native_rows = [p for p in rows if p.startswith(prefix)]
    cmake_list = modules[name]["source_cpp_paths"] if name in modules else cmake_sources[name]
    if name == "DirtySDK":
        # Only the source list declared INTERFACE by the pinned CMake target is a linked provider.
        cmake_list = cmake_sources[name]
    candidate_defs = [(sym, prov) for sym, provs in definitions.items() for prov in provs if prov["family"] == name]
    family_undefs = {sym: refs for sym, refs in undefined.items() if name_family(sym) == ("PPMalloc/CoreAllocator" if name in ("PPMalloc", "CoreAllocator") else name)}
    families[name] = {"direct_git_cpp_count": count_paths(prefix),
                      "tracked_cpp_source_paths": sorted(p for p in tracked if p.startswith(prefix)),
                      "compile_report_rows": len(native_rows),
                      "compile_report_pass": sum(1 for p in native_rows if rows[p]["passed"]),
                      "compile_report_fail": sum(1 for p in native_rows if not rows[p]["passed"]),
                      "declared_cmake_provider_sources": cmake_list,
                      "cmake_sources_present_in_compile_report": [p for p in cmake_list if p in report_paths],
                      "cmake_sources_with_verified_01f2_objects": [p for p in cmake_list if p in object_hashes],
                      "definitions_in_current_objects": len(candidate_defs),
                      "undefined_symbol_names_by_family_prefix": len(family_undefs),
                      "undefined_symbol_references_by_family_prefix": sum(len(v) for v in family_undefs.values()),
                      "undefined_names_without_a_current_object_definition": sorted(s for s in family_undefs if s not in definitions)[:80],
                      "verified_native_objects": [{"source": p, "source_blob_oid": tracked_blob_oids.get(p),
                                                    "object_path": rows[p].get("object"),
                                                    "object_sha256": object_hashes[p], "machine": "0x01F2"}
                                                   for p in native_rows if p in object_hashes],
                      "status": None}

families["EABase"]["status"] = "Header-only submodule target. No missing implementation object is implied by its CMake target; declarations/inline definitions come from pinned headers."
families["EASTL"]["status"] = "CMake target compiles source/*.cpp from gitlink 5e207079; none are in the root native compile sweep. Native PPC/BE objects for this target remain unvalidated."
families["EAThread"]["status"] = "CMake target compiles source/*.cpp from gitlink 38576cbe; none are in the root native compile sweep. Native PPC/BE objects for this target remain unvalidated; check platform dispatch before treating the generic source set as a Xenon implementation."
families["PPMalloc"]["status"] = "The pinned project target is INTERFACE-only (headers); EAGeneralAllocator.cpp has a verified PPC/BE object and its GeneralAllocator symbols satisfy the current passing-object references. It is not named by PPMalloc CMake target sources, so include it explicitly in a source-object link manifest or add it to the native target after confirming the intended provider."
families["CoreAllocator"]["status"] = "INTERFACE target injects icoreallocator_interface.cpp; its current native report row passes and has a hash-verified PPC/BE object."
families["DirtySDK"]["status"] = "INTERFACE target injects 12 common src/*.cpp files; all pass with verified PPC/BE objects. 126 distinct DirtySDK-style names are referenced by passing src/ objects, but only four are defined by the current successful object set; many NetConn/Lobby/VoIP/GameManager APIs have no source provider in the current object set. The direct tracked tree has only common plus PC C++ files and no native Xbox provider directory. This is the clearest provider-family gap to resolve: add the original XDK DirtySDK archive/source set or recover the required native source providers; keep unmatched names provisional until failed engine objects and external libraries are resolved."
families["zlib"]["status"] = "STATIC target names seven C sources; all seven are present in the report and have verified PPC/BE objects. This is actual source/object coverage, not proof of a full link."
families["Lua"]["status"] = "No Lua target/source is declared by vendor/CMakeLists.txt, but 15 distinct Lua API names are referenced by passing src/ objects and each has a current compiled provider in the individual vendor/lua/src runtime objects. Those objects have PPC/BE hashes but are not CMake-linked; add the exact runtime source set or record its approved native provider. Keep all.c, lua.c, luac.c, min.c, noparser.c, and print.c as the established exact amalgamation/tool/alternate exclusions."
families["RenderWare"]["status"] = "The vendor/renderware CMake target is INTERFACE-only; 84 vendor RenderWare TUs have verified PPC/BE objects and 15 fail, but the target declares no implementation sources. The specific engine-reference/provider pairs are listed in family_symbol_reference_evidence; include needed reconstructed src/ providers or exact vendor TUs in the link selection without assuming the whole compile sweep is a link set."

# Compact unresolved references are evidence for provider investigation, not a link verdict.
family_ref_rows = {}
for fam in ("EAThread", "PPMalloc/CoreAllocator", "DirtySDK", "zlib", "Lua", "EASTL", "RenderWare"):
    names = {n: refs for n, refs in undefined.items() if name_family(n) == fam}
    engine_names = {n: refs for n, refs in names.items() if any(p.startswith("src/") for p in refs)}
    family_ref_rows[fam] = {"all_successful_object_undefined_symbol_names": len(names),
                            "all_successful_object_reference_count": sum(len(v) for v in names.values()),
                            "engine_src_undefined_symbol_names": len(engine_names),
                            "engine_src_reference_count": sum(sum(1 for p in refs if p.startswith("src/")) for refs in engine_names.values()),
                            "engine_names_defined_in_current_successful_objects": sum(1 for n in engine_names if n in definitions),
                            "engine_names_without_current_successful_object_definition": {n: sorted(p for p in refs if p.startswith("src/"))
                                                                                           for n, refs in engine_names.items() if n not in definitions},
                            "engine_name_provider_records": {n: [{**p, "source_blob_oid": tracked_blob_oids.get(p["source"])}
                                                                   for p in definitions[n] if p["source"].startswith(("src/", "vendor/"))]
                                                              for n in engine_names if n in definitions}}
    if fam == "RenderWare":
        provider_values = family_ref_rows[fam]["engine_name_provider_records"]
        family_ref_rows[fam]["engine_names_with_vendor_renderware_provider"] = sum(
            any(p["source"].startswith("vendor/renderware/") for p in providers)
            for providers in provider_values.values())
        family_ref_rows[fam]["engine_names_only_vendor_renderware_provider"] = sum(
            any(p["source"].startswith("vendor/renderware/") for p in providers)
            and not any(p["source"].startswith("src/") for p in providers)
            for providers in provider_values.values())

lua_exact_tool_exclusions = {"vendor/lua/etc/all.c", "vendor/lua/etc/min.c", "vendor/lua/etc/noparser.c",
                             "vendor/lua/src/lua.c", "vendor/lua/src/luac.c", "vendor/lua/src/print.c"}
lua_runtime_candidates = sorted(p for p in tracked if p.startswith("vendor/lua/src/") and p.endswith(".c")
                                and p not in lua_exact_tool_exclusions)

# Pin CMake and submodule source evidence to their exact commit objects.
cmake_summary = {p: {k: v for k, v in row.items() if k != "text"} for p, row in cmake.items()}
submodule_manifest = json.loads(AUDIT.read_text(encoding="utf-8"))["external_inputs"]["submodules"]

result = {
    "schema": "native-x360-external-provider-coverage-v1",
    "source_head": HEAD,
    "source_repo_object_database": str(SOURCE_REPO),
    "source_read_method": "Every root source/CMake read uses git show/tree at exact d3fc commit; no intake worktree file was read.",
    "report": str(REPORT), "report_sha256": report_sha, "terminal_audit": str(AUDIT),
    "terminal_audit_sha256": audit_sha, "patch_metadata": str(PATCH_META),
    "patch_metadata_sha256": patch_meta_sha, "patch_sha256": audit["patch_sha256"],
    "requested": report["requested"], "passed": report["passed"], "failed": report["failed"],
    "hash_verified_01f2_objects_scanned": objects_scanned,
    "missing_engine_source_candidates_preserved": report["failed"],
    "tracked_root_cpp_c_count": len(tracked),
    "terminal_include_dependency_snapshot_file_count": len(audit["source_snapshot"]["hashes"]),
    "xdk_defaultlib_request_counts": dict(defaults),
    "xdk_defaultlib_archives": archive_evidence,
    "prior_archive_proof_source": str(OLD_INV),
    "prior_archive_proof_sha256": sha_file(OLD_INV),
    "submodule_terminal_binding": submodule_manifest,
    "submodule_targets": modules,
    "pinned_cmake_evidence": cmake_summary,
    "build_and_submodule_manifest_evidence": tool_evidence,
    "pinned_cmake_search_results": cmake_search,
    "provider_families": families,
    "lua_exact_excluded_tool_and_alternate_sources": sorted(lua_exact_tool_exclusions),
    "lua_runtime_provider_source_candidates": [{"source": p, "source_blob_oid": tracked_blob_oids[p],
                                                 "object_sha256": object_hashes.get(p)} for p in lua_runtime_candidates],
    "family_symbol_reference_evidence": family_ref_rows,
    "all_undefined_external_symbol_name_count": len(undefined),
    "all_undefined_external_reference_count": sum(len(v) for v in undefined.values()),
    "unmatched_names_in_current_successful_object_set_count": len(unresolved_names),
    "unmatched_names_in_current_successful_object_set_sample": sorted(unresolved_names)[:100],
    "missing_engine_source_candidates": [],
    "limits": ["No link was performed.", "Undefined references in successful objects are a partial set: 239 native engine source candidates still lack objects.", "A missing definition among current objects is not by itself a definite link failure; XDK archive members and missing engine objects may supply it.", "Only five default-library archives were hash-checked; cached symbol evidence was reused only where archive SHA matched, and no archive symbol rescan was performed.", "CMake describes declared target sources but does not prove those targets have emitted PPC/BE objects or that every library API used by ARTIST is implemented."],
}

for source, row in rows.items():
    if row["passed"]:
        continue
    log_path = Path(row["log"])
    log_sha = sha_file(log_path)
    expected_log_sha = bound_objects[source]["log_sha256"]
    assert log_sha == expected_log_sha, (source, log_sha, expected_log_sha)
    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^.*(?:fatal error|error C)\d+.*$", log_text, re.M)
    diagnostic = match.group(0).strip() if match else (log_text.splitlines() or ["no diagnostic"])[0]
    result["missing_engine_source_candidates"].append({
        "source": source, "source_blob_oid": tracked_blob_oids.get(source),
        "compile_returncode": row.get("returncode"), "object_is_valid_for_this_report": False,
        "log_path": str(log_path), "log_sha256": log_sha, "first_diagnostic": diagnostic[:500],
    })
assert len(result["missing_engine_source_candidates"]) == report["failed"]

OUT.mkdir(parents=True, exist_ok=True)
(OUT / "native-provider-coverage.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"output": str(OUT / "native-provider-coverage.json"),
                  "report_sha256": report_sha, "objects_scanned": objects_scanned,
                  "defaultlibs": dict(defaults), "unresolved_external_names": len(unresolved_names),
                  "family_reference_counts": {k: v["all_successful_object_undefined_symbol_names"] for k, v in family_ref_rows.items()},
                  "families": {k: {"compiled": v["compile_report_pass"], "failed": v["compile_report_fail"],
                                    "cmake_sources": len(v["declared_cmake_provider_sources"]),
                                    "unresolved_prefix_names": v["undefined_symbol_names_by_family_prefix"]}
                               for k, v in families.items()}}, indent=2))
