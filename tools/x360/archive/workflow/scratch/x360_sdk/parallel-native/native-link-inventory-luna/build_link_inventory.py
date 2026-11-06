#!/usr/bin/env python3
"""Build a source-hash-bound native game link selection proposal from published 8d8881."""
import collections, hashlib, json, os, re, struct, subprocess, tarfile, tempfile, io
from pathlib import Path

HEAD = "8d8881e97a97809b57736dd7a6f39c076fe228ae"
GIT = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp")
OUT = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native-link-inventory-luna")
EFFECTIVE = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/native-parallel-wave4-final3-effective.json")
ARTIFACT_MANIFEST = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/wave4_integrator_sol/native-parallel-wave4-final3-artifact-manifest.json")
LUA_PROOF = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/wave4_integrator_sol/native-lua-game-link-exclusion-proof.json")
SDK = Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK")
ARTIST = Path("D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX")

LUA_EXCLUSIONS = {
    "vendor/lua/etc/all.c": "Lua amalgamation duplicates the individually selected Lua core/library sources and includes lua.c's standalone interpreter main; integration proof binds 658 duplicate external definitions.",
    "vendor/lua/src/lua.c": "Lua 5.1 standalone interpreter entrypoint; defines main and is not the Burnout game entrypoint.",
    "vendor/lua/src/luac.c": "Lua bytecode compiler tool entrypoint; defines main.",
    "vendor/lua/etc/min.c": "Lua minimal standalone interpreter; source declares its own main and only loads stdin.",
    "vendor/lua/etc/noparser.c": "Alternate no-parser Lua core; source says it replaces llex/lparser and object defines duplicate luaX_init/luaY_parser providers.",
    "vendor/lua/src/print.c": "Lua bytecode printer used by the luac tool (src/Makefile's LUAC_O), not a game runtime provider.",
}
LAYOUT_PROBES = {
    "src/GameSource/GameState/ModeManager/BrnModeManager_AssertLayout.cpp": "Source banner identifies a compile-only layout oracle, never called or mounted in the shipped tree.",
    "src/GameSource/GameState/ModeManager/BrnModeManager_EmbedGate.cpp": "Source banner identifies a compile-only embedder/caller gate and says it is not part of the shipped tree.",
    "src/GameSource/Physics/BrnPhysicsModule_layout_check.cpp": "Source says this TU emits nothing at link time; it exists only to force compile-time static_assert checks.",
    "src/GameSource/Physics/VehicleManager/BrnVehicleManager_layout_check.cpp": "Source says its never-called static-member body contains only static_asserts and is a layout gate.",
    "src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics_layout_check.cpp": "Dedicated compile-time layout check; not a runtime body provider.",
    "src/GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics_layout_check.cpp": "Dedicated compile-time layout check; not a runtime body provider.",
    "src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics_layout_check.cpp": "Dedicated compile-time layout check; not a runtime body provider.",
}


def sha(data): return hashlib.sha256(data).hexdigest()
def sha_file(path):
    h=hashlib.sha256()
    with open(path,"rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""): h.update(block)
    return h.hexdigest()
def git_out(args): return subprocess.check_output(["git","-C",str(GIT),*args],stdin=subprocess.DEVNULL)
def norm_path(p): return p.replace("\\","/")

# Read exact tracked C/C++ source blobs from the published commit, without using a checkout.
tree=git_out(["ls-tree","-r","-z",HEAD])
blob_rows=[]
for rec in tree.split(b"\0"):
    if not rec: continue
    meta,path=rec.split(b"\t",1); mode,typ,oid=meta.split(b" ",2); p=path.decode("utf-8")
    if not p.lower().endswith((".c",".cpp")): continue
    blob_rows.append((p,oid.decode("ascii")))
cat_input=b"".join(oid.encode()+b"\n" for _,oid in blob_rows)
cat=subprocess.run(["git","-C",str(GIT),"cat-file","--batch"],input=cat_input,stdout=subprocess.PIPE,check=True).stdout
sources={}; pos=0
for p,expected_oid in blob_rows:
    nl=cat.find(b"\n",pos); header=cat[pos:nl].decode("ascii").split(); oid,kind,size=header[0],header[1],int(header[2]); pos=nl+1
    data=cat[pos:pos+size]; pos+=size+1
    if oid!=expected_oid or kind!="blob": raise RuntimeError("git cat-file blob binding mismatch: "+p)
    sources[p]={"source_sha256":sha(data),"git_blob_oid":oid,"bytes":data}

r=json.load(open(EFFECTIVE,encoding="utf-8")); am=json.load(open(ARTIFACT_MANIFEST,encoding="utf-8")); lua=json.load(open(LUA_PROOF,encoding="utf-8"))
rows={norm_path(x["source"]):x for x in r["results"]}
artifacts=am["artifact_manifest"]
if am["head"]!=HEAD or am["effective_report_sha256"]!=sha_file(EFFECTIVE): raise RuntimeError("effective report/manifest head or hash mismatch")
if not am.get("input_invariance_verified"): raise RuntimeError("published artifact manifest did not verify input invariance")
if set(rows)-set(sources): raise RuntimeError("effective compile row not present in published source tree")
if set(sources)-set(rows):
    unexpected=[p for p in sources if p.startswith(("src/","vendor/")) and "/pc/" not in p.lower() and p not in rows]
    if unexpected: raise RuntimeError("unreported production source(s): "+repr(unexpected[:10]))


def is_pc(p): return any(part.lower()=="pc" for part in p.split("/"))
def is_test(p): return p.startswith("tests/")
entries=[]; error_counts=collections.Counter(); error_examples=collections.defaultdict(list); error_messages={}; object_integrity=[]
for p,s in sorted(sources.items()):
    row=rows.get(p)
    if is_test(p):
        selection="exclude"; reason="Standalone test/evidence source under tests/; not a production game source or part of the XDK compile-all roots (src/, vendor/)."
        group="test_harness"
    elif is_pc(p):
        selection="exclude"; reason="Unbuilt PC-only source (a path component is pc); repository platform instructions retain it as unbuilt reference and tools/build_x360.py filters all pc components."
        group="unbuilt_pc"
    elif row is None:
        selection="exclude"; reason="Outside the source roots selected by the native compile inventory; not attested as a game provider."
        group="outside_native_roots"
    elif p in LUA_EXCLUSIONS:
        selection="exclude"; reason=LUA_EXCLUSIONS[p]; group="lua_tool_or_alternate_provider"
    elif p in LAYOUT_PROBES:
        selection="exclude"; reason=LAYOUT_PROBES[p]; group="compile_only_layout_or_embed_probe"
    else:
        selection="include"; reason="Production src/vendor native source candidate. Retained regardless of compile failure or unresolved bodies; compile failure means its object is currently unavailable, not that the engine code is omittable."
        group="native_game_candidate"
    ent={"source":p,"source_sha256":s["source_sha256"],"git_blob_oid":s["git_blob_oid"],"size_bytes":len(s["bytes"]),"selection":selection,"reason_code":group}
    if group in ("lua_tool_or_alternate_provider","compile_only_layout_or_embed_probe"): ent["evidence_reason"]=reason
    if row:
        ent.update({"compile_passed":bool(row["passed"]),"compile_returncode":row.get("returncode"),"build_head":row.get("build_head"),"object_path_reported":row.get("object")})
        if row["passed"]:
            a=artifacts.get(p,{})
            expected=a.get("object_sha256")
            obj=row["object"]
            actual=sha_file(obj) if os.path.isfile(obj) else None
            ent.update({"object_sha256":expected,"object_file_exists":actual is not None,"object_hash_matches_artifact_manifest":actual==expected,"linkable_object_candidate":selection=="include" and actual==expected})
            if actual!=expected: object_integrity.append({"source":p,"expected":expected,"actual":actual})
        else:
            log=row.get("log"); diag=None
            if log and os.path.isfile(log):
                text=open(log,encoding="utf-8",errors="replace").read()
                m=re.search(r"error C\d+:.*",text)
                diag=(m.group(0) if m else "\n".join(text.splitlines()[:2]))[:400]
                if m:
                    code=re.search(r"error (C\d+):",m.group(0)).group(1)
                    error_counts[code]+=1
                    error_messages.setdefault(code,m.group(0)[:300])
                    if len(error_examples[code])<3: error_examples[code].append(p)
            ent.update({"object_sha256":None,"object_file_exists":bool(row.get("object") and os.path.isfile(row["object"])),"linkable_object_candidate":False,"first_compile_diagnostic":diag})
    else:
        ent.update({"compile_passed":None,"build_head":None,"object_sha256":None,"object_file_exists":False,"linkable_object_candidate":False})
    entries.append(ent)

if object_integrity: raise RuntimeError("object hash drift versus published manifest: "+repr(object_integrity[:3]))

# Parse actual PPC COFF external definitions in the proposed, currently available object subset.
def coff_parse(blob):
    if len(blob)<20: raise ValueError("truncated COFF object")
    machine,nsec,ts,ptr,nsym,opt,flags=struct.unpack_from("<HHIIIHH",blob,0)
    if machine!=0x01f2: raise ValueError("wrong COFF machine: %x"%machine)
    st=ptr+nsym*18 if ptr else 0
    str_len=struct.unpack_from("<I",blob,st)[0] if ptr and st+4<=len(blob) else 0
    def sym_name(raw):
        if raw[:4]==b"\0\0\0\0":
            ix=struct.unpack_from("<I",raw,4)[0]
            if 4<=ix<str_len:
                end=blob.find(b"\0",st+ix)
                return blob[st+ix:end].decode("utf-8","replace")
        return raw.split(b"\0",1)[0].decode("latin1")
    sections=[]; base=20+opt
    for i in range(nsec):
        o=base+i*40; rawname=blob[o:o+8].split(b"\0",1)[0].decode("latin1")
        if rawname.startswith("/") and rawname[1:].isdigit() and str_len:
            ix=int(rawname[1:]); end=blob.find(b"\0",st+ix); rawname=blob[st+ix:end].decode("utf-8","replace")
        size,rawptr=struct.unpack_from("<II",blob,o+16); prel=struct.unpack_from("<I",blob,o+24)[0]; nrel=struct.unpack_from("<H",blob,o+32)[0]; secflags=struct.unpack_from("<I",blob,o+36)[0]
        sections.append({"name":rawname,"size":size,"rawptr":rawptr,"relptr":prel,"nrel":nrel,"flags":secflags,"selection":0})
    symbols=[]; names_by_index={}; i=0
    while i<nsym:
        o=ptr+i*18; raw=blob[o:o+18]; name=sym_name(raw); val=struct.unpack_from("<I",raw,8)[0]; sec=struct.unpack_from("<h",raw,12)[0]; typ=struct.unpack_from("<H",raw,14)[0]; cls=raw[16]; aux=raw[17]
        names_by_index[i]=name
        if cls==3 and sec>0 and sec<=nsec and aux and name.startswith("."):
            sections[sec-1]["selection"]=blob[o+18:o+36][14]
        if cls==2 and sec>0 and sec<=nsec:
            symbols.append({"name":name,"section":sec,"value":val,"type":typ,"class":cls})
        i+=1+aux
    return {"machine":machine,"sections":sections,"symbols":symbols,"ptrsym":ptr,"nsym":nsym,"names_by_index":names_by_index,"string_base":st,"string_len":str_len}

def body_signature(blob,coff,secno):
    sec=coff["sections"][secno-1]; start=sec["rawptr"]; raw=blob[start:start+sec["size"]]
    relocs=[]
    for j in range(sec["nrel"]):
        o=sec["relptr"]+j*10; va,symi,typ=struct.unpack_from("<IIH",blob,o); target=coff["names_by_index"].get(symi,"<unknown>"); relocs.append((va,target,typ))
    return sha(raw+b"\0"+json.dumps(relocs,separators=(",",":"),ensure_ascii=True).encode()),sec,relocs

selected=[]; defaultlib_counts=collections.Counter(); definitions=collections.defaultdict(list); object_hash_groups=collections.defaultdict(list); entrypoint_defs=[]; drectve_examples={}
for ent in entries:
    if not ent.get("linkable_object_candidate"): continue
    p=ent["source"]; obj=rows[p]["object"]
    blob=open(obj,"rb").read(); cf=coff_parse(blob); object_hash_groups[sha(blob)].append(p); selected.append(ent)
    for m in re.findall(rb"/DEFAULTLIB\s*:\s*\"?([^\"\x00\s]+)",blob,re.I): defaultlib_counts[m.decode("ascii","replace").upper()]+=1
    for sym in cf["symbols"]:
        body,sec,relocs=body_signature(blob,cf,sym["section"])
        symbol_data=blob[sec["rawptr"]+sym["value"]:sec["rawptr"]+min(sec["size"],sym["value"]+8)]
        prov={"source":p,"source_sha256":ent["source_sha256"],"object_path":obj,"object_sha256":ent["object_sha256"],"symbol":sym["name"],"section":sec["name"],"section_number":sym["section"],"section_flags_hex":hex(sec["flags"]),"comdat_selection":sec["selection"],"symbol_value_hex":hex(sym["value"]),"symbol_data_hex":symbol_data.hex().upper(),"section_size":sec["size"],"section_body_reloc_sha256":body,"relocation_count":len(relocs)}
        definitions[sym["name"]].append(prov)
        if sym["name"].lower().lstrip("_") in {"main","maincrtstartup","winmain","wmain","dllmain"}: entrypoint_defs.append(prov)

dups={n:v for n,v in definitions.items() if len(v)>1}
any_groups=[]; nodup_groups=[]; strong_groups=[]
for name,providers in dups.items():
    comdat=[bool(int(x["section_flags_hex"],16)&0x1000) for x in providers]
    sels=[x["comdat_selection"] for x in providers]
    distinct=len({x["section_body_reloc_sha256"] for x in providers})
    group={"symbol":name,"provider_count":len(providers),"body_variant_count":distinct,"providers":providers}
    if not all(comdat): strong_groups.append(group)
    elif any(x==1 for x in sels): nodup_groups.append(group)
    else: any_groups.append(group)

lua_proof_exclusions=lua["exclusions"]
# Subsystem and build driver source evidence.
allfiles={}
for p in ["tools/build_x360.py","tools/prepare_x360_linker.py","README.md","AGENTS.md","vendor/lua/etc/all.c","vendor/lua/etc/min.c","vendor/lua/etc/noparser.c","vendor/lua/src/lua.c","vendor/lua/src/luac.c","vendor/lua/src/print.c","vendor/lua/src/Makefile","src/GameSource/GameState/ModeManager/BrnModeManager_AssertLayout.cpp","src/GameSource/GameState/ModeManager/BrnModeManager_EmbedGate.cpp","src/GameSource/Physics/BrnPhysicsModule_layout_check.cpp","src/GameSource/Physics/VehicleManager/BrnVehicleManager_layout_check.cpp","src/GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics_layout_check.cpp","src/GameSource/Physics/VehicleManager/VehiclePhysics/TrafficPhysics_layout_check.cpp","src/GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics_layout_check.cpp"]:
    if p in sources: allfiles[p]=sources[p]["source_sha256"]

# Find default CRT/runtime libraries requested by the actual PPC COFF .drectve sections.
# Parse local XDK archives at COFF-symbol level to bind CRT entries to a library/member.
def archive_symbol_members(path,wanted):
    b=open(path,"rb").read(); pos=8; raw_members=[]; longnames=b""
    while pos+60<=len(b):
        hdr=b[pos:pos+60]
        if hdr[58:60]!=b"`\n": break
        name=hdr[:16].decode("latin1").strip(); size=int(hdr[48:58].decode("ascii").strip() or "0"); start=pos+60; data=b[start:start+size]
        if name=="//": longnames=data
        raw_members.append((name,data)); pos=start+size+(size&1)
    out=[]
    for name,data in raw_members:
        if name=="//" or name=="/" or not data or len(data)<20: continue
        if name.startswith("/") and name[1:].isdigit():
            ix=int(name[1:]); end=longnames.find(b"\0",ix); resolved=longnames[ix:end].decode("latin1") if end>=0 else name
        else: resolved=name.rstrip("/")
        if struct.unpack_from("<H",data,0)[0]!=0x01f2: continue
        try: cf=coff_parse(data)
        except Exception: continue
        if any(s["name"].lower()==wanted.lower() for s in cf["symbols"]): out.append({"member":resolved,"symbol":wanted,"machine":"0x01F2"})
    return out
libdir=SDK/"lib/xbox"
crt_symbol_checks={}
for lib,symbols in {"xapilib.lib":["mainCRTStartup","XapiInitProcess","XapiCallThreadNotifyRoutines","_rtinit","_cinit"],"libcMT.lib":["_mtinit","_cexit"],"libcpMT.lib":[],"xboxkrnl.lib":[]}.items():
    path=libdir/lib
    crt_symbol_checks[lib]={"sha256":sha_file(path),"symbols":{sym:archive_symbol_members(path,sym) for sym in symbols}}

# Native source identity audit for misleading PS3-attributed TUs. Do not exclude on the directory name.
ps3_notes={
"src/GameShared/GameClasses/Development/PerfMon/Gpu/PS3/CgsPerfMonGpuPS3.cpp":{"class":"native_identity_current_body_diverges","note":"The file itself calls its home ARTIST CgsPerfMonGpuX360.cpp, but its current body replaces X360 predicated callbacks with D3D9 GPU timestamps/std::mutex and it fails the XDK compile. Keep as an engine source candidate, mark body/provider incomplete."},
"src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp":{"class":"native_recovered_misattribution","note":"Source identifies this as Native Xenon SaveLoad and includes CgsSaveLoadX360.h; ARTIST body identities are present."},
"src/GameShared/GameClasses/RenderWare/PS3/CgsRwMaterialCRC32ResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"ARTIST 0x828A80E8 CgsResource::RwMaterialCRC32ResourceType::GetTypeID."},
"src/GameShared/GameClasses/RenderWare/PS3/CgsRwRasterResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"Source names ARTIST X360 bodies 0x828A9858, 0x828A88E0 and 0x828A8930; include despite PS3 path and compile failure."},
"src/GameShared/GameClasses/RenderWare/PS3/CgsRwRenderableResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"Source names ARTIST 0x828A8968 CgsResource::RwRenderableResourceType::FixUp; include despite PS3 path and compile failure."},
"src/GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceType.cpp":{"class":"native_recovered_misattribution","note":"Source includes ARTIST-attested CgsRwVertexDescResourceType body and addresses 0x828A8A28/0x82B61260/0x82B637D0."},
"src/GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.cpp":{"class":"native_provider_rehomed_empty_tu","note":"Source says native handler is homed in sibling CgsRwVertexDescResourceType.cpp and the local duplicate definition was retired; keep source row, but it contributes no native external body."},
"src/GameShared/GameClasses/RenderWare/PS3/materialstates/CgsMaterialStateResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"Source names ARTIST X360 CgsResource::MaterialStateResourceType methods and addresses."},
"src/GameShared/GameClasses/RenderWare/PS3/materialstates/CgsRwShaderParameterResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"Source names ARTIST X360 shader-parameter resource methods and addresses."},
"src/GameShared/GameClasses/RenderWare/PS3/materialstates/CgsRwTextureStateResourceTypePS3.cpp":{"class":"native_recovered_misattribution","note":"Source names ARTIST X360 texture-state resource methods and addresses."},
"src/GameShared/GameClasses/System/PS3/CgsHardwareInitPS3.cpp":{"class":"native_recovered_misattribution","note":"Source explicitly says this is the X360 hardware-init TU despite its PS3 pathname; ARTIST addresses include 0x828E05A0 and 0x828D6138."},
"src/GameShared/GameClasses/System/PS3/CgsHardwareLanguagePS3.cpp":{"class":"native_recovered_misattribution","note":"ARTIST 0x828D6938 CgsSystem::HardwareLanguage::GetHardwareLanguage calls XTLGetLanguage and XGetGameRegion."},
"src/GameShared/GameClasses/System/PS3/CgsHardwareSkuPS3.cpp":{"class":"native_recovered_misattribution","note":"ARTIST 0x828D7128 CgsSystem::HardwareSku::FindLanguage; source preserves SKU/language branches."},
"src/GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.cpp":{"class":"native_recovered_misattribution","note":"Source explicitly identifies X360 ARTIST DateAndTime bodies at 0x828D6A68 and following addresses."},
"src/GameSource/GameState/AchievementManager/PS3/BrnGameStateAchievementManagerPS3.cpp":{"class":"native_recovered_misattribution","note":"Source identifies console-attested AchievementManagerBase gameplay-event hooks; matching ARTIST identities exist."},
}
ps3_audit=[]
for ent in entries:
    if "PS3" in ent["source"] or "/ps3/" in ent["source"].lower():
        ps3_audit.append({"source":ent["source"],"source_sha256":ent["source_sha256"],"compile_passed":ent.get("compile_passed"),**ps3_notes.get(ent["source"],{"class":"reviewed_path_name_only_no_exclusion","note":"Retain; PS3 pathname alone is not an exclusion criterion."})})

# Build supported entry/CRT facts from ARTIST, local linker docs, COFF directives and XDK archives.
selected_obj_count=len(selected); external_main=[p for p in definitions.get("main",[]) if p]
# Hash local evidence without invoking the linker or modifying SDK/build trees.
evidence_paths=[EFFECTIVE,ARTIFACT_MANIFEST,LUA_PROOF,Path("C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/link-bound-runtime.txt"),GIT/"CMakeLists.txt",GIT/"tools/build_x360.py",GIT/"tools/prepare_x360_linker.py",GIT/"README.md",SDK/"bin/win32/link.exe"]
evidence={str(p):sha_file(p) for p in evidence_paths if p.is_file()}

compile_failures=[e for e in entries if e.get("compile_passed") is False and e["selection"]=="include"]
pass_selection=[e for e in entries if e.get("linkable_object_candidate")]
summary={
    "source_head":HEAD,
    "effective_compile_report":str(EFFECTIVE),"effective_compile_report_sha256":sha_file(EFFECTIVE),
    "effective_artifact_manifest":str(ARTIFACT_MANIFEST),"effective_artifact_manifest_sha256":sha_file(ARTIFACT_MANIFEST),
    "all_tracked_c_cpp":len(entries),"native_roots_tracked_c_cpp":sum(1 for e in entries if e["source"].startswith(("src/","vendor/"))),
    "effective_compile_inventory_rows":len(rows),"effective_compile_pass":r["passed"],"effective_compile_fail":r["failed"],
    "test_harness_sources_excluded":sum(1 for e in entries if e["reason_code"]=="test_harness"),
    "unbuilt_pc_sources_excluded":sum(1 for e in entries if e["reason_code"]=="unbuilt_pc"),
    "tool_or_alternate_provider_sources_excluded":sum(1 for e in entries if e["reason_code"]=="lua_tool_or_alternate_provider"),
    "compile_only_layout_or_embed_probes_excluded":sum(1 for e in entries if e["reason_code"]=="compile_only_layout_or_embed_probe"),
    "source_candidates_selected":sum(1 for e in entries if e["selection"]=="include"),
    "currently_hash_verified_object_candidates":selected_obj_count,
    "selected_source_candidates_without_usable_object":len(compile_failures),
    "compile_failure_first_diagnostic_counts":error_counts.most_common(20),
    "compile_failure_examples":{k:error_examples[k] for k,_ in error_counts.most_common(20)},
    "compile_failure_representative_messages":{k:error_messages[k] for k,_ in error_counts.most_common(20)},
    "symbol_scan":{
      "objects_scanned":selected_obj_count,
      "exact_duplicate_object_sha_groups":sum(1 for v in object_hash_groups.values() if len(v)>1),
      "defined_external_symbol_names":len(definitions),"duplicate_external_symbol_groups":len(dups),
      "comdat_any_duplicate_groups":len(any_groups),"comdat_any_groups_with_different_section_or_relocation_body_hashes":sum(1 for x in any_groups if x["body_variant_count"]>1),
      "comdat_noduplicates_groups":len(nodup_groups),"noncomdat_duplicate_groups":len(strong_groups),
      "entrypoint_symbols_in_selected_game_objects":entrypoint_defs,
      "hard_or_manual_provider_conflict_symbols":[x["symbol"] for x in nodup_groups+strong_groups],
    },
    "renderware_base_resource_descriptor":next((e for e in entries if e["source"]=="vendor/renderware/src/rw/BaseResourceDescriptor.cpp"),None),
    "ps3_named_source_audit":ps3_audit,"ps3_named_source_attribution_counts":dict(collections.Counter(x["class"] for x in ps3_audit)),"ps3_path_only_exclusions":0,
    "missing_game_bodies":[{"identity":"main","artist_address":"0x827E60D8","evidence":"XDK mainCRTStartup at ARTIST 0x8292B480 calls main(argc,argv,envp); no global main definition exists in selected native source/object set after proven tool entrypoint exclusions."}],
    "crt_and_link_support":{"artist_crt_entry":"mainCRTStartup @0x8292B480","artist_init":"XapiInitProcess @0x82934940","verified_linker_flags":["/MACHINE:PPCBE","/SUBSYSTEM:XBOX","/ENTRY:mainCRTStartup"],"object_default_libraries":dict(defaultlib_counts),"xdk_archive_symbol_evidence":crt_symbol_checks,"linker_preparation_script":"tools/prepare_x360_linker.py copies the local linker and binds its host CRT manifest; it does not perform a game link.","strict_link_command_status":"not_supportable: 374 selected engine source candidates have no validated object, original game main body is absent, three NODUPLICATES duplicate body groups and two strong/COMDAT duplicate data pairs remain, and no full unresolved-symbol set exists."},
    "evidence_sha256":evidence,
}
reason_definitions={
"native_game_candidate":"Production src/vendor source in the native compile inventory; retained even on compile failure. A failed TU is pending, not excluded.",
"test_harness":"Tracked test/evidence source under tests/; outside the production src/vendor roots and not part of the game compile inventory.",
"unbuilt_pc":"Unbuilt PC-only source (path component pc); README/AGENTS retain it as reference and tools/build_x360.py filters pc components.",
"lua_tool_or_alternate_provider":"Evidence-backed Lua amalgamation, standalone tool entrypoint/helper, or alternate core provider; see per-source evidence_reason and Lua provenance block.",
"compile_only_layout_or_embed_probe":"Dedicated compile-only layout/embed gate, not a runtime provider; see per-source evidence_reason.",
"outside_native_roots":"Outside src/vendor production roots and outside the native compile inventory."
}
def compact_group(g):
 return {"symbol":g["symbol"],"provider_count":g["provider_count"],"body_variant_count":g["body_variant_count"],"providers":[{k:p[k] for k in ("source","symbol","section","section_number","section_flags_hex","comdat_selection","symbol_value_hex","symbol_data_hex","section_size","section_body_reloc_sha256","relocation_count")} for p in g["providers"]]}
manifest={"schema":"xbox360-native-game-link-source-selection-proposal-v1","reason_definitions":reason_definitions,"summary":summary,"selection_policy":{"include":"Every published-8d source in src/vendor selected by the native compiler inventory except the evidence-backed standalone/alternate Lua tools and compile-only layout/embed probes. Keep native engine sources even when their compile row fails; mark their object unavailable.","exclude":"Top-level tests/ test harnesses; native-disabled PC path components; exact attested Lua tool/amalgamation/alternate providers; seven source files proven to be compile-only probes. No exclusion is based only on a PS3 pathname, compile failure, or unresolved symbol.","production_source_roots":["src/","vendor/"]},"runtime_source_entries":entries,"selected_link_object_candidate_paths":[e["source"] for e in pass_selection],"duplicate_external_symbol_groups":{"noduplicates":[compact_group(x) for x in nodup_groups],"noncomdat":[compact_group(x) for x in strong_groups],"comdat_any_summary":{"group_count":len(any_groups),"different_body_signature_group_count":sum(1 for x in any_groups if x["body_variant_count"]>1),"body_mismatch_groups":[compact_group(x) for x in any_groups if x["body_variant_count"]>1],"body_signature_method":"SHA-256 over raw section bytes plus sorted COFF relocations (virtual address, target symbol, type); differing signatures are flagged for review, not treated as safe duplicate bodies."}},"lua_game_link_proof":{"source_head":lua["source_head"],"status":lua["status"],"runtime_provider_count":len(lua["separate_runtime_providers"]),"all_c_duplicates":lua["duplicate_external_definition_count"],"previously_attested_exclusions":[{"source":x["source"],"source_sha256":x["source_sha256"],"reason":x["reason"]} for x in lua["exclusions"]],"additional_attested_tool_exclusions":[{"source":p,"source_sha256":sources[p]["source_sha256"],"reason":LUA_EXCLUSIONS[p]} for p in ["vendor/lua/etc/min.c","vendor/lua/etc/noparser.c","vendor/lua/src/print.c"]]},"strict_link_command_proposal":{"status":"not_supportable_yet","command":None,"verified_flags":["/MACHINE:PPCBE","/SUBSYSTEM:XBOX","/ENTRY:mainCRTStartup"],"verified_automatic_runtime_libraries":dict(defaultlib_counts),"reason":"A concrete link command must not hide/force the current missing native objects, absent game main body or duplicate providers. The linker command line can be finalized after those exact source/body blockers are repaired and a complete unresolved symbol inventory is generated."}}
manifest["unpublished_upcoming_entry_candidate"]={"source":"src/GameSource/Main/BrnMainX360.cpp","commits":["0ad32289270e450e18682d36e6e3c2631647ec77","be3d3199c774f4a82d951cd404aa3ea440049dcc"],"status":"Not in published-8d selection. Root reports a real native main(int,char**) body with gpBurnoutGame and ARTIST initialization order; current candidate fails only the retained sizeof(0x9A1300) guard.","policy":"Keep separate from the published-8d manifest until that candidate source and object are published and re-audited."}

OUT.mkdir(parents=True,exist_ok=True)
(OUT/"native-link-inventory.json").write_text(json.dumps(manifest,indent=1,ensure_ascii=False)+"\n",encoding="utf-8")
(OUT/"inventory-summary.json").write_text(json.dumps(summary,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
print(json.dumps({"manifest":str(OUT/"native-link-inventory.json"),"summary":str(OUT/"inventory-summary.json"),"sources":summary["all_tracked_c_cpp"],"candidate_sources":summary["source_candidates_selected"],"selected_objects":summary["currently_hash_verified_object_candidates"],"pending_objects":summary["selected_source_candidates_without_usable_object"],"hard_conflicts":len(nodup_groups)+len(strong_groups),"ps3_sources":len(ps3_audit)},indent=2))
