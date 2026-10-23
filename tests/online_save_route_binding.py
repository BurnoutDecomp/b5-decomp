"""Exact source composition for the hosted route-save fixture; no subprocesses."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re

MAIN = "src/GameSource/Gui/BrnGuiOptionsDataProfile.cpp"
HEADER = "src/GameSource/Gui/BrnGuiOptionsDataProfile.h"
GETTERS = "src/GameSource/Gui/BrnGuiOptionsDataProfile_wS34_00.cpp"
PARAMS = "src/GameSource/Gui/Events/BrnGuiEventNetworkGameParams.cpp"
EVENTS = "src/GameSource/GameState/BrnGameStateSharedIO.cpp"
CACHE = "src/GameSource/Gui/BrnGuiCache.cpp"
STRSTREAM = "src/GameShared/GameClasses/Development/CgsStrStream.cpp"
DIAGNOSTIC_GLOBALS = "tests/OnlineSaveRouteDiagnosticGlobals.cpp"
EVIDENCE = "tests/evidence/online_save_route"
BASELINE = EVIDENCE + "/BrnGuiOptionsDataProfile_8b2940d4.cpp"
BASE = "8b2940d40e76fd6a88a37615f6163a99c2d5e3b8"
BASE_BLOB = "9695cfcadd8abd5ba5bdeb319afe09721d3f8251"

def identity(path: Path) -> dict:
    raw = path.read_bytes(); lf = raw.replace(b"\r\n", b"\n")
    return {"raw_sha256": hashlib.sha256(raw).hexdigest(), "raw_bytes": len(raw),
            "lf_sha256": hashlib.sha256(lf).hexdigest(), "lf_bytes": len(lf)}

def read(path: Path) -> str:
    return path.read_bytes().replace(b"\r\n", b"\n").decode("utf-8-sig")

def definition_span(source: str, signature: str) -> tuple[int, int]:
    matches = list(re.finditer(r"(?m)^[ \t]*" + re.escape(signature), source))
    if len(matches) != 1:
        raise ValueError("requires one actual definition: " + signature)
    start = matches[0].end() - len(signature)
    depth = 0; opened = False
    tokens = r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{};]'
    for token in re.finditer(tokens, source[start:]):
        if token[0] == ";" and not opened:
            raise ValueError("declaration is not a body: " + signature)
        if token[0] == "{": depth += 1; opened = True
        elif token[0] == "}":
            depth -= 1
            if opened and depth == 0: return start, start + token.end()
    raise ValueError("unterminated body: " + signature)

def definition(source: str, signature: str) -> str:
    start, end = definition_span(source, signature)
    return source[start:end]

def validate_sources(b5: Path, workflow: Path) -> dict:
    pin_path = b5 / EVIDENCE / "source_pins.json"
    pins = json.loads(pin_path.read_text("utf-8"))
    if (pins.get("schema_version") != 1 or pins.get("normalization") != "crlf_to_lf_only" or
        pins.get("base") != BASE or pins.get("baseline_git_blob_sha1") != BASE_BLOB):
        raise ValueError("unsupported source-pin contract")
    expected = {
        MAIN, HEADER, GETTERS, PARAMS, EVENTS, CACHE, STRSTREAM, DIAGNOSTIC_GLOBALS,
        "src/GameShared/GameClasses/Development/CgsStrStream.h",
        "src/GameShared/GameClasses/Development/Log/CgsLog.h",
        "src/GameSource/Gui/BrnGuiCache.h", "src/GameSource/Gui/BrnGuiEventTypeDefs.h",
        "src/GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h",
        "src/GameSource/GameState/BrnGameStateSharedIO.h", "src/GameSource/GameState/BrnGameStateTypes.h",
        "src/GameShared/GameClasses/Core/CgsAssert.h", "src/GameShared/GameClasses/Core/CgsID.h",
        "src/types.hpp", "tests/OnlineSaveRoute.cpp", "tests/run_online_save_route.py",
        "tests/online_save_route_binding.py", "tests/online_save_route_owned.py",
        BASELINE, EVIDENCE + "/test_plan.json",
    }
    if set(pins.get("b5_files", {})) != expected:
        raise ValueError("source pins must cover exact fixture/body/API composition")
    tools = {"tools/build/msvc_flags.txt", "tools/build/msvc_includes.txt", "tools/build/msvc_env.bat"}
    if set(pins.get("workflow_files", {})) != tools:
        raise ValueError("canonical MSVC tool pins required")
    rows = {}
    for label, root, values in (("b5", b5, pins["b5_files"]), ("workflow", workflow, pins["workflow_files"])):
        for relative, want in values.items():
            path = (root / relative).resolve()
            if not path.is_relative_to(root) or not path.is_file(): raise ValueError("invalid bound path: " + relative)
            if set(want) != {"lf_sha256", "lf_bytes"} or not re.fullmatch(r"[0-9a-f]{64}", want["lf_sha256"]):
                raise ValueError("invalid pin row: " + relative)
            got = identity(path)
            rows[label + "/" + relative] = {"path": str(path), **got}
            if any(got[key] != value for key, value in want.items()): raise ValueError("input changed: " + relative)
    old = (b5 / BASELINE).read_bytes().replace(b"\r\n", b"\n")
    blob = hashlib.sha1(b"blob " + str(len(old)).encode() + b"\0" + old).hexdigest()
    if blob != BASE_BLOB: raise ValueError("oldtrap source is not the original 8b Git blob")
    cache_ctor = definition(read(b5 / CACHE), "GuiCache::GuiCache()")
    old_ctor = definition(read(b5 / BASELINE), "void OptionsDataProfile::Construct()")
    definitions = {"cache_constructor": cache_ctor, "original_options_constructor": old_ctor}
    for name, body in definitions.items():
        digest = hashlib.sha256(body.encode()).hexdigest()
        if pins.get("definitions", {}).get(name) != digest: raise ValueError("bound definition changed: " + name)
    return {"pin_file": {"path": str(pin_path), **identity(pin_path)}, "files": rows,
            "baseline_git_blob_sha1": blob, "definition_sha256": pins["definitions"]}

def compose_variant(b5: Path, dest: Path, variant: str) -> dict:
    """Whole subject TU; one exact cache ctor extraction; one bounded old-ctor graft."""
    if variant not in ("candidate", "oldtrap", "omitted_init"): raise ValueError("unknown variant")
    constructor = definition(read(b5 / CACHE), "GuiCache::GuiCache()")
    provider = dest / "CacheConstructor.cpp"
    provider.write_text('#include <cstring>\n#include "GameSource/Gui/BrnGuiCache.h"\n'
                        'namespace BrnGui {\n' + constructor + '\n}\n', encoding="utf-8", newline="\n")
    selected = b5 / MAIN; recipe = "exact whole production main TU"
    definition_receipt = {"cache_constructor_sha256": hashlib.sha256(constructor.encode()).hexdigest()}
    if variant == "oldtrap":
        selected = b5 / BASELINE; recipe = "exact original whole 8b main TU, candidate header"
    elif variant == "omitted_init":
        current = read(b5 / MAIN); previous = read(b5 / BASELINE)
        start, end = definition_span(current, "void OptionsDataProfile::Construct()")
        old_ctor = definition(previous, "void OptionsDataProfile::Construct()")
        selected = dest / "OptionsDataProfile_omitted_init.cpp"
        selected.write_text(current[:start] + old_ctor + current[end:], encoding="utf-8", newline="\n")
        recipe = "candidate whole TU with only Construct definition replaced by exact original Construct"
        definition_receipt.update({"candidate_replaced_span": [start, end],
            "old_constructor_sha256": hashlib.sha256(old_ctor.encode()).hexdigest(),
            "replaced_candidate_constructor_sha256": hashlib.sha256(current[start:end].encode()).hexdigest()})
    return {"sources": [b5 / "tests/OnlineSaveRoute.cpp", selected, b5 / GETTERS, b5 / PARAMS, b5 / EVENTS, provider,
                        b5 / STRSTREAM, b5 / DIAGNOSTIC_GLOBALS],
            "main_source": {"path": str(selected), **identity(selected)},
            "constructor_provider": {"path": str(provider), **identity(provider)},
            "recipe": recipe, "definitions": definition_receipt}
