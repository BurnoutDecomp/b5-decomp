#!/usr/bin/env python3
"""Bounded read-only link-preparation audit over the frozen R4 XDK object report."""
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

OUT = Path(__file__).resolve().parent
HEAD = "d81ba6aa1ab01fa5d321dea3f0ae45aeb5d27647"
SOURCE_REPO = Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
CANONICAL = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp")
R4_ROOT = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r4")
REPORT = R4_ROOT / "full-stage1" / "compile-results.json"
AUDIT = R4_ROOT / "full-stage1-terminal-audit.json"
PATCH_META = R4_ROOT / "native-parallel-wave8-r4-full-stage1-patch.json"
LINK_BOUND = Path(r"C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\link-bound-runtime.txt")
TOOLHOST = CANONICAL / "build/x360/tool-host"
SDK = CANONICAL / "build/x360/sdk-6534.4/portable/XDK"
W6_COVERAGE = Path(r"D:\Reverse\BP-X360-Experimental-Artifacts\wave6\native-provider-coverage-luna\native-provider-coverage.json")

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

def source_row(record, source_oids, object_hashes):
    return {"source": record, "source_blob_oid": source_oids.get(record),
            "source_sha256": sha(git_blob(record)) if record in source_oids else None,
            "object_path": rows[record].get("object") if record in rows else None,
            "object_sha256": object_hashes.get(record),
            "machine": "0x01F2" if record in object_hashes else None}

def family(name):
    lo = name.lower()
    if lo in ("main", "_main", "maincrtstartup"):
        return "entry"
    if lo.startswith("lua"):
        return "Lua"
    if lo.startswith(("inflate", "deflate", "crc32", "adler32", "compress", "uncompress", "gz")):
        return "zlib"
    if re.match(r"^(?:\?\?[01])?(?:[^@]+@)?(?:thread|semaphore|mutex|rwmutex|condition|barrier|futex|threadlocalstorage)@thread@ea@@", lo):
        return "EAThread"
    if re.match(r"^(?:\?\?[01])?(?:[^@]+@)?(?:icoreallocator|generalallocator)@allocator@ea@@", lo):
        return "PPMalloc/CoreAllocator"
    if lo == "crypt_argchk":
        return "LibTomCrypt"
    if lo in {"socket", "bind", "listen", "accept", "closesocket", "recv", "send", "setsockopt", "ioctlsocket", "xnetstartup"}:
        return "XDKNetwork"
    if lo.startswith(("dirty", "netconn", "netlib", "netgame", "prot", "lobby", "crypt", "voip", "pingmanager", "gamemanager", "telemetry")):
        return "DirtySDK"
    return None

def coff_probe(blob):
    if len(blob) < 20:
        raise ValueError("truncated COFF")
    machine, nsec, _, ptrsym, nsym, opt, _ = struct.unpack_from("<HHIIIHH", blob, 0)
    if machine != 0x01F2 or opt != 0:
        raise ValueError(f"not native PPC/BE COFF: machine={machine:#x}, optional={opt}")
    strtab = ptrsym + nsym * 18 if ptrsym else 0
    strsize = struct.unpack_from("<I", blob, strtab)[0] if ptrsym and strtab + 4 <= len(blob) else 0

    def sym_name(raw):
        if raw[:4] == b"\0\0\0\0":
            index = struct.unpack_from("<I", raw, 4)[0]
            if 4 <= index < strsize:
                end = blob.find(b"\0", strtab + index)
                return blob[strtab + index:end].decode("utf-8", "replace")
        return raw[:8].split(b"\0", 1)[0].decode("latin1")

    sections, base = [], 20 + opt
    for i in range(nsec):
        off = base + 40 * i
        n = blob[off:off + 8].split(b"\0", 1)[0].decode("latin1")
        size, rawptr = struct.unpack_from("<II", blob, off + 16)
        sections.append((n, size, rawptr))
    ext, index = [], 0
    while index < nsym:
        off = ptrsym + 18 * index
        raw = blob[off:off + 18]
        name = sym_name(raw)
        value = struct.unpack_from("<I", raw, 8)[0]
        sec = struct.unpack_from("<h", raw, 12)[0]
        storage, aux = raw[16], raw[17]
        if storage == 2:
            ext.append((name, value, sec))
        index += 1 + aux
    directives = b"\n".join(blob[p:p + size] for n, size, p in sections if n == ".drectve")
    defaults = [x.decode("ascii", "replace").upper()
                for x in re.findall(rb"/DEFAULTLIB\s*:\s*\"?([^\"\x00\s]+)", directives, re.I)]
    return ext, defaults

report_sha, audit_sha, patch_meta_sha = sha_file(REPORT), sha_file(AUDIT), sha_file(PATCH_META)
report = json.loads(REPORT.read_text(encoding="utf-8"))
audit = json.loads(AUDIT.read_text(encoding="utf-8"))
patch_meta = json.loads(PATCH_META.read_text(encoding="utf-8"))
assert audit["head"] == HEAD and audit["report_sha256"] == report_sha
assert audit["metadata_sha256"] == patch_meta_sha
assert patch_meta["head"] == HEAD and patch_meta["sha256"] == audit["patch_sha256"]
assert (report["requested"], report["passed"], report["failed"]) == (3616, 3098, 518)
rows = {r["source"]: r for r in report["results"]}
bound = audit["artifact_manifest"]
assert len(rows) == 3616 and set(rows) == set(bound)

# Pin the root source/CMake tree at d81; never read files from the moved intake checkout.
tree = git(SOURCE_REPO, "ls-tree", "-r", "-z", HEAD)
source_oids, gitlinks = {}, {}
for rec in tree.split(b"\0"):
    if not rec:
        continue
    meta, rawpath = rec.split(b"\t", 1)
    mode, typ, oid = meta.decode().split(" ", 2)
    path = rawpath.decode("utf-8")
    if mode == "160000":
        gitlinks[path] = oid
    elif path.lower().endswith((".cpp", ".c")):
        source_oids[path] = oid
assert set(rows) <= set(source_oids)

# Verify every object against the immutable R4 terminal binder. Extract only the
# default-library directives, entry symbols, and the requested provider families.
defaults = Counter()
definitions = defaultdict(lambda: defaultdict(list))
references = defaultdict(lambda: defaultdict(list))
object_hashes = {}
passes = 0
for source, row in rows.items():
    if not row["passed"]:
        continue
    obj = Path(row["object"])
    digest = sha_file(obj)
    expected = bound[source]["object_sha256"]
    assert digest == expected, (source, digest, expected)
    blob = obj.read_bytes()
    symbols, libs = coff_probe(blob)
    object_hashes[source] = digest
    defaults.update(libs)
    for name, value, sec in symbols:
        fam = family(name)
        if fam is None:
            continue
        record = {"source": source, "source_blob_oid": source_oids.get(source),
                  "object_sha256": digest, "machine": "0x01F2"}
        if sec > 0 or (sec == 0 and value > 0) or sec == -1:
            definitions[fam][name].append(record)
        elif sec == 0 and value == 0:
            references[fam][name].append(source)
    passes += 1
assert passes == report["passed"]

src_family_summary = {}
for fam in sorted(set(references) | set(definitions)):
    refs = references[fam]
    defs = definitions[fam]
    game_names = {name: sorted(set(path for path in owners if path.startswith("src/")))
                  for name, owners in refs.items() if any(path.startswith("src/") for path in owners)}
    unresolved = {name: owners for name, owners in game_names.items() if name not in defs}
    src_family_summary[fam] = {
        "current_undefined_symbol_names": len(refs),
        "current_undefined_references": sum(len(v) for v in refs.values()),
        "engine_src_undefined_names": len(game_names),
        "engine_src_names_defined_in_current_objects": sum(1 for n in game_names if n in defs),
        "engine_src_names_without_current_object_definition": unresolved,
        "engine_src_names_and_current_providers": {
            n: {"referring_sources": owners, "providers": defs.get(n, [])}
            for n, owners in game_names.items()},
    }

def rows_for(predicate):
    return [source_row(p, source_oids, object_hashes) for p in sorted(rows) if predicate(p)]

c_rows = [p for p in rows if p.endswith(".c")]
cpp_rows = [p for p in rows if p.endswith(".cpp")]
assert len(c_rows) + len(cpp_rows) == len(rows)
counts = {
    "C": {"requested": len(c_rows), "passed": sum(rows[p]["passed"] for p in c_rows),
          "failed": sum(not rows[p]["passed"] for p in c_rows)},
    "C++": {"requested": len(cpp_rows), "passed": sum(rows[p]["passed"] for p in cpp_rows),
            "failed": sum(not rows[p]["passed"] for p in cpp_rows)},
}
assert counts["C"] == {"requested": 42, "passed": 42, "failed": 0}
assert counts["C++"] == {"requested": 3574, "passed": 3056, "failed": 518}

# Required source object sets that the native compile sweep does and does not cover.
lua_tool_excludes = {"vendor/lua/etc/all.c", "vendor/lua/etc/min.c", "vendor/lua/etc/noparser.c",
                     "vendor/lua/src/lua.c", "vendor/lua/src/luac.c", "vendor/lua/src/print.c"}
lua_runtime = sorted(p for p in c_rows if p.startswith("vendor/lua/src/") and p not in lua_tool_excludes)
zlib_sources = sorted(p for p in c_rows if p.startswith("vendor/zlib/src/"))
ea_thread_engine = sorted(p for p in rows if p.startswith("src/SDKs/EATech/eathread/"))
ppmalloc_source = "vendor/PPMalloc/source/EAGeneralAllocator.cpp"
assert len(lua_runtime) == 29 and len(zlib_sources) == 7 and ppmalloc_source in rows

modules = {}
for name in ("EABase", "EASTL", "EAThread"):
    pin = gitlinks["vendor/" + name]
    modrepo = CANONICAL / "vendor" / name
    # Terminal audit bound the submodule checkout and clean status at R4.
    prior = audit["external_inputs"]["submodules"][name]
    assert prior["gitlink"] == pin and prior["head"] == pin and prior["status"] == ""
    text = git(modrepo, "show", f"{pin}:CMakeLists.txt")
    names = git(modrepo, "ls-tree", "-r", "--name-only", pin).decode().splitlines()
    source_cpp = sorted("vendor/" + name + "/" + p for p in names
                        if p.startswith("source/") and p.endswith(".cpp") and "/" not in p[len("source/"):])
    modules[name] = {"gitlink": pin, "cmake_sha256": sha(text), "source_cpp_paths": source_cpp,
                     "source_cpp_count": len(source_cpp),
                     "reported_XDK_objects": [p for p in source_cpp if p in object_hashes]}

def blob_evidence(paths):
    result = {}
    for p in paths:
        data = git_blob(p)
        result[p] = {"blob_oid": source_oids.get(p) or git(SOURCE_REPO, "rev-parse", f"{HEAD}:{p}").decode().strip(),
                     "sha256": sha(data)}
    return result

cmake_files = ["CMakeLists.txt", "src/CMakeLists.txt", "src/GameSource/CMakeLists.txt",
               "src/GameShared/CMakeLists.txt", "vendor/CMakeLists.txt", "vendor/PPMalloc/CMakeLists.txt",
               "vendor/coreallocator/CMakeLists.txt", "vendor/dirtysdk/CMakeLists.txt",
               "vendor/dirtysdk/src/CMakeLists.txt", "vendor/zlib/CMakeLists.txt",
               "vendor/renderware/CMakeLists.txt"]
cmake_evidence = {}
for p in cmake_files:
    raw = git_blob(p)
    cmake_evidence[p] = {"blob_oid": git(SOURCE_REPO, "rev-parse", f"{HEAD}:{p}").decode().strip(), "sha256": sha(raw),
                         "text": raw.decode("utf-8", "replace")}

# Reuse previously audited XDK archive member evidence only after matching current SHA-256.
old_cov = json.loads(W6_COVERAGE.read_text(encoding="utf-8"))
old_libs = old_cov["xdk_defaultlib_archives"]
sdk_libs = {"OLDNAMES": "oldnames.lib", "LIBCMT": "libcMT.lib", "LIBCPMT": "libcpMT.lib",
            "XAPILIB": "xapilib.lib", "XBOXKRNL": "xboxkrnl.lib"}
archives = {}
for directive, filename in sdk_libs.items():
    path = SDK / "lib/xbox" / filename
    digest = sha_file(path)
    prior = old_libs.get(directive)
    if prior:
        assert digest == prior["sha256"], (filename, prior["sha256"], digest)
        symbol_evidence = prior.get("prior_symbol_evidence")
        reused = True
    else:
        symbol_evidence, reused = None, False
    archives[directive] = {"file": filename, "path": str(path), "sha256": digest,
                            "objects_requesting": defaults[directive],
                            "reused_prior_symbol_evidence_after_hash_match": reused,
                            "known_symbol_evidence": symbol_evidence}

# One additional bounded archive-member check for the exact Winsock/XNet imports
# named in the reconstructed X360 GameTalk provider; no archive members are written.
network_archive_path = SDK / "lib/xbox/xnet.lib"
network_targets = {"socket", "bind", "listen", "accept", "closesocket", "recv", "send",
                   "setsockopt", "ioctlsocket", "XNetStartup"}

def archive_member_symbols(path, targets):
    blob = Path(path).read_bytes()
    found = defaultdict(list)
    pos = 8
    longnames = b""
    members = []
    while pos + 60 <= len(blob):
        hdr = blob[pos:pos + 60]
        if hdr[58:60] != b"`\n":
            break
        rawname = hdr[:16].decode("latin1").strip()
        size = int(hdr[48:58].decode("ascii").strip() or "0")
        start = pos + 60
        data = blob[start:start + size]
        if rawname == "//":
            longnames = data
        elif rawname not in ("/", "/SYM64/"):
            if rawname.startswith("/") and rawname[1:].isdigit():
                ix = int(rawname[1:]); end = longnames.find(b"\0", ix)
                member = longnames[ix:end].decode("latin1") if end >= 0 else rawname
            else:
                member = rawname.rstrip("/")
            members.append((member, data))
        pos = start + size + (size & 1)
    for member, data in members:
        if len(data) >= 20 and data[:4] == b"\0\0\xff\xff":
            machine = struct.unpack_from("<H", data, 4)[0]
            size_data = struct.unpack_from("<I", data, 10)[0]
            name = data[20:20 + size_data].split(b"\0", 1)[0].decode("latin1", "replace")
            normalized = name.lstrip("_").lower()
            if normalized in {x.lower() for x in targets}:
                found[normalized].append({"member": member, "symbol": name, "machine": hex(machine), "kind": "import-object"})
            continue
        try:
            symbols, _ = coff_probe(data)
        except Exception:
            continue
        for name, value, sec in symbols:
            normalized = name.lstrip("_").lower()
            if normalized in {x.lower() for x in targets} and (sec > 0 or (sec == 0 and value > 0) or sec == -1):
                found[normalized].append({"member": member, "symbol": name, "machine": "0x01F2", "kind": "COFF-definition"})
    return found

network_archive_sha = sha_file(network_archive_path)
network_archive_members = archive_member_symbols(network_archive_path, network_targets)

# Validate the already-prepared host linker and its original SDK identity by hashes only.
host_json = TOOLHOST / "linker-host.json"
host = json.loads(host_json.read_text(encoding="utf-8"))
prepared_link = TOOLHOST / "link.exe"
original_link = SDK / "bin/win32/link.exe"
assert sha_file(prepared_link) == host["prepared_sha256"]
assert sha_file(original_link) == host["original_sha256"]
bound_text = LINK_BOUND.read_text(encoding="utf-8", errors="replace")
assert "/MACHINE:PPCBE" in bound_text and "/SUBSYSTEM:XBOX" in bound_text and "/ENTRY:symbol" in bound_text
link_options = {flag: flag in bound_text for flag in (
    "/MACHINE:PPCBE", "/SUBSYSTEM:XBOX", "/ENTRY:symbol", "/XEX[:NO]", "/XEXCONFIG:filename", "/LIBPATH:dir")}

link_bound_sha = sha_file(LINK_BOUND)
tool_evidence = {}
for path in ("tools/build_x360.py", "tools/prepare_x360_linker.py", "src/types.hpp", ".gitmodules"):
    data = git_blob(path)
    tool_evidence[path] = {"blob_oid": git(SOURCE_REPO, "rev-parse", f"{HEAD}:{path}").decode().strip(),
                           "sha256": sha(data), "text": data.decode("utf-8", "replace")}

all_main_object_providers = definitions.get("entry", {}).get("main", [])
main_object_providers = {
    "game_main": [p for p in all_main_object_providers if p["source"] == "src/GameSource/Main/BrnMainX360.cpp"],
    "standalone_lua_tool_mains": [p for p in all_main_object_providers if p["source"].startswith("vendor/lua/")],
    "other_object_mains": [p for p in all_main_object_providers
                            if p["source"] != "src/GameSource/Main/BrnMainX360.cpp" and not p["source"].startswith("vendor/lua/")],
}
assert len(main_object_providers["game_main"]) == 1
provider_cmake_search = subprocess.run(
    ["git", "-C", str(SOURCE_REPO), "grep", "-n", "-i", "-E", "-e",
     r"BrnEAThreadX360|eathread_(semaphore|rwmutex)[.]cpp|EAGeneralAllocator[.]cpp|vendor/lua|add_subdirectory\(lua\)",
     HEAD, "--", "**/CMakeLists.txt", "*.cmake"],
    stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert provider_cmake_search.returncode in (0, 1)
xex_cmake_search = subprocess.run(
    ["git", "-C", str(SOURCE_REPO), "grep", "-n", "-i", "-E", "-e",
     r"XEXCONFIG|[.]xexconfig|/XEX", HEAD, "--", "**/CMakeLists.txt", "*.cmake", "*.py", "*.cmd", "*.bat", "*.md"],
    stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert xex_cmake_search.returncode in (0, 1)

manifest = {
    "schema": "frozen-r4-x360-native-link-preparation-v1",
    "source_head": HEAD,
    "patch_sha256": audit["patch_sha256"],
    "patch_metadata": str(PATCH_META), "patch_metadata_sha256": patch_meta_sha,
    "report": str(REPORT), "report_sha256": report_sha,
    "terminal_audit": str(AUDIT), "terminal_audit_sha256": audit_sha,
    "source_read_method": "Root source and CMake text were read as Git blobs at d81. EA submodule text was read from the exact d81 gitlink commits. No moved intake worktree source was read.",
    "source_snapshot_input_files": len(audit["source_snapshot"]["hashes"]),
    "compile_counts": counts,
    "compiler_mode_evidence": {"driver": "tools/build_x360.py accepts .c and .cpp; the R4 command uses cl.exe /c and /FItypes.hpp.",
                               "explicit_TC_TP_flag_present_in_report": any(
                                   arg.upper() in ("/TC", "/TP") for row in report["results"] for arg in row["command"]),
                               "mode_rule": "MSVC selects C for .c and C++ for .cpp when neither /TC nor /TP is supplied.",
                               "C_files_all_pass": counts["C"]["failed"] == 0,
                               "forced_include": "/FItypes.hpp",
                               "types_hpp": tool_evidence["src/types.hpp"]},
    "all_C_sources": rows_for(lambda p: p.endswith(".c")),
    "lua_runtime_C_source_candidates": rows_for(lambda p: p in lua_runtime),
    "lua_exact_tool_or_alternate_exclusions": sorted(lua_tool_excludes),
    "zlib_target_C_sources": rows_for(lambda p: p in zlib_sources),
    "ea_thread_custom_X360_source_rows": rows_for(lambda p: p in ea_thread_engine),
    "ppmalloc_source_candidate": source_row(ppmalloc_source, source_oids, object_hashes),
    "provider_symbol_family_scan": src_family_summary,
    "xdk_network_archive_symbol_check": {"archive": str(network_archive_path), "sha256": network_archive_sha,
                                         "target_symbols": sorted(network_targets),
                                         "providers": {k: v for k, v in network_archive_members.items()},
                                         "scope": "Only target symbols referenced by CgsGameTalkProtocolX360.cpp were checked in xnet.lib; archive members stayed in memory."},
    "entry_symbol_provider": main_object_providers,
    "submodule_provider_sources": modules,
    "source_and_CMake_evidence": {"CMake": {p: {k: v for k, v in val.items() if k != "text"}
                                                 for p, val in cmake_evidence.items()},
                                   "build_tools_and_types": {p: {k: v for k, v in val.items() if k != "text"}
                                                              for p, val in tool_evidence.items()},
                                   "CMake_target_link_libraries": [line.strip() for line in cmake_evidence["src/CMakeLists.txt"]["text"].splitlines()
                                                                    if "target_link_libraries" in line],
                                   "CMake_declared_provider_gaps": {
                                       "EAThread": "CMake builds source/*.cpp from a gitlink; those files are absent from the 3616 root compile rows.",
                                       "EASTL": "CMake builds source/*.cpp from a gitlink; those files are absent from the 3616 root compile rows.",
                                       "EABase": "Header-only CMake target.",
                                       "Lua": "No Lua target or source list in vendor/CMakeLists; runtime .c objects are in compile rows but not linked by that target.",
                                       "PPMalloc": "INTERFACE-only CMake target; EAGeneralAllocator.cpp passes in the report but is not a target source.",
                                       "DirtySDK": "The target adds 12 common sources; no native Xbox source subtree is in the tracked source inventory.",
                                       "RenderWare": "Interface-only target; vendor/renderware .cpp objects are compile candidates, not target sources."},
                                   "launch_disabled_game_source_directories": ["src/GameSource/Physics", "src/GameSource/Sound", "src/GameSource/World"],
                                   "CMake_provider_source_search_results": provider_cmake_search.stdout.decode("utf-8", "replace").splitlines(),
                                   "pinned_XEX_config_source_search_results": xex_cmake_search.stdout.decode("utf-8", "replace").splitlines(),
                                   "tracked_DirtySDK_cpp_paths": sorted(p for p in source_oids if p.startswith("vendor/dirtysdk/") and p.endswith(".cpp")),
                                   "tracked_DirtySDK_xbox_or_xenon_cpp_paths": sorted(p for p in source_oids if p.startswith("vendor/dirtysdk/") and p.endswith(".cpp") and re.search(r"(^|/)(xbox|xenon)(/|_)", p, re.I))},
    "XDK_default_library_requests": dict(defaults),
    "XDK_archive_hashes_and_cached_symbol_evidence": archives,
    "cached_Wave6_archive_proof": {"path": str(W6_COVERAGE), "sha256": sha_file(W6_COVERAGE),
                                    "reuse_policy": "Every current XDK archive SHA matched the cached proof; no archive symbol scan was repeated."},
    "prepared_linker": {"prepared_linker_host_json": str(host_json), "host_json_sha256": sha_file(host_json),
                        "prepared_linker": str(prepared_link), "prepared_linker_sha256": sha_file(prepared_link),
                        "original_XDK_linker": str(original_link), "original_XDK_linker_sha256": sha_file(original_link),
                        "link_bound_runtime_file": str(LINK_BOUND), "link_bound_runtime_sha256": link_bound_sha,
                        "verified_options": link_options, "accepted_XEX_options": ["/XEX", "/XEXCONFIG:<file>"],
                        "do_not_run_original_unprepared_linker": True},
    "minimal_native_image_plan": {
        "target": "Burnout5 executable/XEX; main source now compiles in frozen R4",
        "object_sources": "The complete launch-required engine source closure, including Physics, Sound, and World (currently commented out in GameSource/CMakeLists), plus the provider sources enumerated above. Do not equate the 3616-row compile sweep or current CMake slice with the final link object set.",
        "entry": "/ENTRY:mainCRTStartup (verified XAPILIB startup provider from hash-matched prior symbol evidence)",
        "subsystem": "/SUBSYSTEM:XBOX",
        "machine": "/MACHINE:PPCBE",
        "output_format": "XDK linker supports /XEX and /XEXCONFIG, but the exact project XEX config/output settings are not declared by build_x360.py or the pinned CMake files.",
        "library_search": str(SDK / "lib/xbox"),
        "default_libraries": "Use the five verified XDK archive files and .drectve requests listed above. CgsGameTalkProtocolX360.cpp is a compile-sweep-only TU, not an active CMake source; if explicitly selected, add xnet.lib (verified to provide all ten declared Winsock/XNet imports). No /FORCE or unresolved-symbol suppression.",
        "readiness": "Not ready: 518 C++ source rows have no validated native object; DirtySDK coverage is incomplete; several required provider objects are absent from the active CMake target; Physics, Sound, and World are commented out; and no XEX config/output settings are declared by build_x360.py or pinned CMake files.",
    },
    "limitations": ["No link/build/SDK utility was run.",
                    "The R5 report is running and was not read or used.",
                    "Only selected external symbol families and .drectve library requests were scanned; there is no complete unresolved-symbol list.",
                    "The 518 failed source rows remain candidates and may supply providers missing from the passing object subset.",
                    "Cached XDK archive member proof was reused only after SHA-256 equality; no archive was rescanned.",
                    "This audit does not establish gameplay, XEX launch, or complete runtime coverage."],
}

OUT.mkdir(parents=True, exist_ok=True)
(OUT / "native-link-preparation.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"output": str(OUT / "native-link-preparation.json"),
                  "report_sha256": report_sha, "rows": report["requested"],
                  "pass": report["passed"], "fail": report["failed"],
                  "objects_hash_verified": passes, "compile_counts": counts,
                  "default_libraries": dict(defaults),
                  "family_refs": {k: {"names": v["engine_src_undefined_names"],
                                      "current_definitions": v["engine_src_names_defined_in_current_objects"]}
                                  for k, v in src_family_summary.items()}}, indent=2))
