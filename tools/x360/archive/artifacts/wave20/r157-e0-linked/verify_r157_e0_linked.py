"""Bind R157 Send01 E0 objects and CRT evidence to the exact strict-linked PE."""
from collections import defaultdict
import hashlib, json, re, struct, subprocess
from pathlib import Path

HERE=Path(__file__).resolve().parent
WAVE=HERE.parent
LINK=WAVE/"r157-link"
SOURCE_ROOT=Path(r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake")
HEAD="e08c99a8a0902da7998408f8e324843ec86a3fae"
TREE="41e8700b3dbef3308250b2da6c0be090e9563b0a"
DONOR="16522f872cb2a0c52141cd45e82a370d74d9180e"
PINS={
 "seal.json":"c9136354e375d71c6f3b699d8f38eb380a47e6aae3be9d854ce7eae88474262a",
 "invocation.json":"8907884633081dc2da0f7edc6d8fd66cf3f6229c830ac57fc8f7b877ec0f0cce",
 "result.json":"8e72a98ac54b2c47e3d7ff426c758b4353c8a099720d158f412b43ff2efe669d",
 "native-game.pe":"b4a46715e3efe619a41d1c3a3eca722d4b83f6b09dd96319ea6de54725494c2c",
 "native-game.map":"745fcf3b95284b7095fb5f0f6a54794417261d773a77b87b8e6372cb51cfb853",
 "native-game.exe":"e3e08607afc9128495061a5956c9985353b96e33bcdf103abe8b39b1bffe5684",
}
E0_HEADER="src/GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"
E0_PROVIDER="src/GameSource/Sound/Module/BrnRootSoundModule.cpp"
E0_READERS=[
 "src/GameSource/Sound/Collision/BrnCollisionEffect.cpp",
 "src/GameSource/Sound/Global/BrnGlobalStateManager.cpp",
 "src/GameSource/Sound/Global/BrnHUDEffect.cpp",
 "src/GameSource/Sound/Global/BrnPresentationEffect.cpp",
 "src/GameSource/Sound/Passby/BrnPassbyEffect.cpp",
 "src/GameSource/Sound/Streaming/BrnStreamingEffect.cpp",
 "src/GameSource/Sound/Vehicles/Deformation/BrnDeformationEffect.cpp",
 "src/GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.cpp",
 "src/GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.cpp",
 "src/GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.cpp",
]
E4_SOURCES=[
 "src/GameSource/Sound/Vehicles/Engines/BrnBoostEffect.cpp",
 "src/GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.cpp",
 "src/GameSource/Sound/Vehicles/Engines/BrnDualGinsuExhaustEffect.cpp",
 "src/GameSource/Sound/Vehicles/Engines/BrnSweetenersEffect.cpp",
]
KDEFAULT="?K_DefaultSendName@Playback@CgsSound@@3UName@12@B"
MAKEHASH="?MakeHash@Name@Playback@CgsSound@@SAIPBD@Z"
CONNECT="?Connect@Voice@Logic@CgsSound@@QAAXII@Z"
CREATE="?Create@VoiceWrapper@Logic@CgsSound@@QAAXABUCreateParams@123@@Z"
GETGAIN="?GetGain@VoiceWrapper@Logic@CgsSound@@QBAMPBH@Z"
SETGAIN="?SetGain@Voice@Logic@CgsSound@@QAAXIMPBI@Z"
ARTIST_DIR=Path(r"D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX")
ARTIST_REPORT=WAVE/"r156-send01-e0"/"REPORT.md"
ARTIST_REPORT_SHA="9917e111c804bf8c61b9b7dd340b721f00ad5886dfbec1aced7a46360df69f4f"
SOURCE_PINS=WAVE/"r156-send01-e0"/"source-pins.json"
SOURCE_PINS_SHA="4cafba057b29b4e63f30b2735f733912cf3ea7d01bba4626000c51362345767c"
ARTIST_E0_EXPORTS=[
 ("0x826EEA68","BrnSound::Logic::Streaming::StreamingEffect::Detach",1),
 ("0x826F0B00","BrnSound::Logic::Traffic::TrafficSkid::Attach",1),
 ("0x826F37E8","BrnSound::Vehicles::Deformation::DeformationEffect::Attach",1),
 ("0x826F4440","BrnSound::Vehicles::Wheels::InAirEffect::PlayJumpCamLanding",1),
 ("0x826F4598","BrnSound::Vehicles::Wheels::InAirEffect::Attach",1),
 ("0x826F4268","BrnSound::Vehicles::Wheels::InAirEffect::PlayLanding",1),
 ("0x826F4D20","BrnSound::Vehicles::Wheels::AISkidEffect::Attach",1),
 ("0x826F5BF8","BrnSound::Logic::GlobalStateManager::Prepare",2),
 ("0x826F78D8","BrnSound::Logic::PresentationEffect::Play",2),
 ("0x826F6338","BrnSound::Logic::HUDEffect::PlaySound",1),
 ("0x826F5F68","BrnSound::Logic::HUDEffect::Notify",1),
 ("0x826F9860","BrnSound::Logic::Passby::PassbyEffect::Prepare",1),
]
ARTIST_RAW_ONLY=["0x8269E4DC","0x826E89F8"]
ARTIST_E4_EXPORTS=[
 ("0x8269A308","BrnSound::Vehicles::Engines::BoostEffect::UpdateBoostStream",1),
 ("0x826F2690","BrnSound::Vehicles::Engines::DualGinsuExhaustEffect::Attach",1),
 ("0x826F14C0","BrnSound::Vehicles::Engines::DualGinsuEffect::Attach",2),
 ("0x826F2DD8","BrnSound::Vehicles::Engines::SweetenersEffect::Notify",1),
 ("0x826F2880","BrnSound::Vehicles::Engines::BoostEffect::Attach",1),
 ("0x826F3620","BrnSound::Vehicles::Engines::SweetenersEffect::UpdateDamageBangs",1),
]
METHODS=[
 dict(key="collision_prepare",source=E0_READERS[0],prefix="?Prepare@CollisionEffect@Collision@Logic@BrnSound@@",
      artist=["0x826E89F8"],offset=0x10c,calls=[CONNECT]),
 dict(key="global_prepare",source=E0_READERS[1],prefix="?Prepare@GlobalStateManager@Logic@BrnSound@@",
      artist=["0x826F5DA0","0x826F5E10"],offset=0x1d8,calls=[CONNECT,SETGAIN,MAKEHASH]),
 dict(key="hud_builder",source=E0_READERS[2],prefix="?BuildSplicerVoiceParams@HUDEffect@Logic@BrnSound@@",
      artist=["0x826F6248","0x826F64C0"],offset=0xc0,calls=[]),
 dict(key="presentation_playstream",source=E0_READERS[3],prefix="?PlayStream@PresentationEffect@Logic@BrnSound@@",
      artist=["0x8269E4DC"],offset=0xb0,
      calls=["?PostStreamRequest@StreamingStateManager@Streaming@Logic@BrnSound@@"]),
 dict(key="presentation_play",source=E0_READERS[3],prefix="?Play@PresentationEffect@Logic@BrnSound@@",
      artist=["0x826F7A98","0x826F7C38"],offset=0x74,calls=[CREATE]),
 dict(key="passby_prepare",source=E0_READERS[4],prefix="?Prepare@PassbyEffect@Passby@Logic@BrnSound@@",
      artist=["0x826F9994"],offset=0x14c,calls=[CONNECT]),
 dict(key="streaming_detach",source=E0_READERS[5],prefix="?Detach@StreamingEffect@Streaming@Logic@BrnSound@@",
      artist=["0x826EEAD8"],offset=0x60,calls=[GETGAIN]),
 dict(key="deformation_attach",source=E0_READERS[6],prefix="?Attach@DeformationEffect@Deformation@Vehicles@BrnSound@@",
      artist=["0x826F3940"],offset=0xf8,calls=[CREATE]),
 dict(key="traffic_skid_attach",source=E0_READERS[7],prefix="?Attach@TrafficSkid@Traffic@Logic@BrnSound@@",
      artist=["0x826F0C20"],offset=0x154,calls=[CREATE]),
 dict(key="ai_skid_attach",source=E0_READERS[8],prefix="?Attach@AISkidEffect@Wheels@Vehicles@BrnSound@@",
      artist=["0x826F4E58"],offset=0x148,calls=[CREATE]),
 dict(key="inair_make_params",source=E0_READERS[9],prefix="?MakeSplicerParams@?A0x0ee522b9@Wheels@Vehicles@BrnSound@@",
      artist=["0x826F431C","0x826F4564"],offset=0x98,calls=[]),
 dict(key="inair_attach",source=E0_READERS[9],prefix="?Attach@InAirEffect@Wheels@Vehicles@BrnSound@@",
      artist=["0x826F46A8"],offset=0x1a8,calls=[CREATE]),
]
CALLERS=[
 dict(key="hud_notify",source=E0_READERS[2],prefix="?Notify@HUDEffect@Logic@BrnSound@@",
      helper="?BuildSplicerVoiceParams@HUDEffect@Logic@BrnSound@@"),
 dict(key="hud_play_sound",source=E0_READERS[2],
      prefix="?PlaySound@HUDEffect@Logic@BrnSound@@",
      helper="?BuildSplicerVoiceParams@HUDEffect@Logic@BrnSound@@"),
 dict(key="inair_play_landing",source=E0_READERS[9],
      prefix="?PlayLanding@InAirEffect@Wheels@Vehicles@BrnSound@@",
      helper="?MakeSplicerParams@?A0x0ee522b9@Wheels@Vehicles@BrnSound@@"),
 dict(key="inair_jumpcam_landing",source=E0_READERS[9],
      prefix="?PlayJumpCamLanding@InAirEffect@Wheels@Vehicles@BrnSound@@",
      helper="?MakeSplicerParams@?A0x0ee522b9@Wheels@Vehicles@BrnSound@@"),
 dict(key="inair_suspension_squeeks",source=E0_READERS[9],
      prefix="?UpdateSuspensionSqueeks@InAirEffect@Wheels@Vehicles@BrnSound@@",
      helper="?MakeSplicerParams@?A0x0ee522b9@Wheels@Vehicles@BrnSound@@"),
]

HELPER=WAVE/"r128-batch"/"verify_matches.py"
PPC={"__file__":str(HELPER)}
exec(HELPER.read_text(encoding="utf-8").split("\nif len(sys.argv)==2:",1)[0],PPC)
fixed_equal=PPC["fixed_equal"]
data_target=PPC["data_target"]
branch_target=PPC["branch_target"]
object_symbol=PPC["object_symbol"]
symbol_literal=PPC["symbol_literal"]
linked_reader=PPC["linked_reader"]
PPC["masks"][2]=0xffffffff

def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream,"sha256").hexdigest()

def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))

def git(*args):
    return subprocess.check_output(
        ["git","--no-optional-locks","-C",str(SOURCE_ROOT),*args]
    ).decode().strip()

def map_index(text):
    rows=[]
    rx=re.compile(r"(?m)^\s+([0-9a-fA-F]{4}):([0-9a-fA-F]+)\s+(\S+)\s+([0-9a-fA-F]{8})\s*(.*?)\s*$")
    for m in rx.finditer(text):
        seg,off,symbol,va,detail=m.groups()
        parts=detail.split()
        rows.append(dict(segment=int(seg,16),offset=int(off,16),symbol=symbol,
                         va=int(va,16),owner=parts[-1] if parts else "",detail=detail))
    return rows

def exact_map(rows,name,owner=None):
    hits=[r for r in rows if r["symbol"]==name and
          (owner is None or r["owner"].lower()==owner.lower())]
    assert len(hits)==1,(name,owner,hits[:4])
    return hits[0]

def mapped_method(rows,prefix,owner):
    hits=[r for r in rows if r["symbol"].startswith(prefix) and
          r["owner"].lower()==owner.lower() and r["segment"]==3]
    assert len(hits)==1,(prefix,owner,hits[:4])
    return hits[0]

def coff_function(path,symbol):
    data=Path(path).read_bytes()
    machine,nsec,_,symtab,nsyms,opt,_=struct.unpack_from("<HHIIIHH",data)
    assert machine==0x1f2 and opt==0
    strings=symtab+18*nsyms
    matches=[]
    i=0
    while i<nsyms:
        pos=symtab+18*i
        raw=data[pos:pos+8]
        if raw[:4]==bytes(4):
            start=strings+struct.unpack_from("<I",raw,4)[0]
            name=data[start:data.index(bytes(1),start)].decode("latin1")
        else:
            name=raw.rstrip(bytes(1)).decode("latin1")
        value,sec,typ,storage,aux=struct.unpack_from("<IhHBB",data,pos+8)
        if name==symbol and 1<=sec<=nsec and storage in (2,3) and (typ&0x20):
            matches.append((name,sec,value,storage))
        i+=1+aux
    assert len(matches)==1,(symbol,matches)
    name,sec,value,storage=matches[0]
    assert value==0,(symbol,value)
    h=struct.unpack_from("<8sIIIIIIHHI",data,20+(sec-1)*40)
    _,_,_,size,raw,reloc,_,nrel,_,flags=h
    assert flags&0x20
    return dict(symbol=name,section=sec,storage=storage,data=data,
                code=data[raw:raw+size],
                relocs=[struct.unpack_from("<IIH",data,reloc+10*j) for j in range(nrel)])


class LinkedImage:
    def __init__(self):
        self.seal=load(LINK/"seal.json")
        self.invocation=load(LINK/"invocation.json")
        self.result=load(LINK/"result.json")
        self.image=load(LINK/"image-verification.json")
        self.diagnostics=load(LINK/"diagnostic-parsed.json")
        self.post_checks=load(LINK/"post-link-checks.json")
        self.source_inventory=load(LINK/"source-inventory.json")
        assert self.diagnostics["unresolved_messages"]==0
        assert self.diagnostics["duplicate_messages"]==[]
        assert self.post_checks and all(self.post_checks.values())
        assert sha(LINK/"source-inventory.json")==self.seal["source_inventory"]["sha256"]
        assert self.source_inventory["head"]==HEAD
        self.native_report=load(self.seal["report"]["path"])
        assert sha(self.seal["report"]["path"])==self.seal["report"]["sha256"]
        assert self.native_report["passed"]==3710 and self.native_report["failed"]==0
        assert self.seal["schema"]=="r157-native-link-seal-v1"
        assert self.seal["head"]==HEAD and self.seal["source_root"]==str(SOURCE_ROOT)
        assert self.image["head"]==HEAD and self.image["magic"]=="XEX2"
        assert self.result["exit_code"]==0 and self.result["all_post_checks_pass"]
        assert self.invocation["sealed_sha256"]==PINS["seal.json"]
        for rel,digest in PINS.items():
            assert sha(LINK/rel)==digest,(rel,sha(LINK/rel))
        self.rows={r["source"]:r for r in self.invocation["objects"]}
        self.signed={r["source"]:r for r in self.seal["selected"]}
        assert self.rows.keys()==self.signed.keys()
        assert len(self.rows)==len(self.signed)==3589
        self.sources=self.seal["sources"]
        self.map_text=(LINK/"native-game.map").read_text(encoding="utf-8",errors="replace")
        self.map_rows=map_index(self.map_text)
        self.address,self.read=linked_reader(LINK)

    def selected_object(self,source):
        row=self.rows[source]
        signed=self.signed[source]
        obj=Path(row["object"])
        assert obj==Path(signed["object"])
        assert sha(obj)==row["sha256"]==signed["sha256"],source
        return obj

    def method(self,source,prefix):
        obj=self.selected_object(source)
        row=mapped_method(self.map_rows,prefix,obj.name)
        extracted=coff_function(obj,row["symbol"])
        linked=self.read(row["va"],len(extracted["code"]))
        fixed_equal(extracted["code"],linked,extracted["relocs"])
        local_abs=[]
        for off,idx,kind in extracted["relocs"]:
            if kind==2:
                target_name,target_value,target_section=object_symbol(extracted["data"],idx)
                assert target_section==extracted["section"],(row["symbol"],target_name,target_section,extracted["section"])
                target_va=row["va"]+target_value
                assert word_be(linked,off)==target_va,(row["symbol"],target_name,hex(word_be(linked,off)),hex(target_va))
                local_abs.append(dict(offset=hex(off),symbol=target_name,target_va=hex(target_va)))
        names={}
        for off,idx,kind in extracted["relocs"]:
            name,_,_=object_symbol(extracted["data"],idx)
            names[(off,kind)]=name
        return dict(source=source,object=str(obj),object_sha256=sha(obj),
                    symbol=row["symbol"],owner=row["owner"],va=row["va"],
                    code=extracted["code"],linked=linked,
                    relocs=extracted["relocs"],names=names,local_absolute_targets=local_abs)

def relocations_for(method,target):
    return [(off,idx,kind) for off,idx,kind in method["relocs"]
            if method["names"].get((off,kind))==target]

def word_be(data,offset):
    return struct.unpack_from(">I",data,offset)[0]

def check_data_ref(method,target,expected_va,expected_low=None):
    hits=relocations_for(method,target)
    high=[off for off,_,kind in hits if kind==0x10]
    low=[off for off,_,kind in hits if kind==0x11]
    assert len(high)==len(low)==1,(method["symbol"],target,hits)
    if expected_low is not None:
        assert low[0]==expected_low,(method["symbol"],low,expected_low)
    actual=data_target(method["linked"],high[0],low[0])
    assert actual==expected_va,(method["symbol"],hex(actual),hex(expected_va))
    return dict(symbol=target,high_reloc=hex(high[0]),low_reloc=hex(low[0]),
                resolved_va=hex(actual))

def resolve_calls(image,method,prefixes):
    result=[]
    for prefix in prefixes:
        hits=[(off,method["names"].get((off,kind),""))
              for off,_,kind in method["relocs"] if kind==6 and
              method["names"].get((off,kind),"").startswith(prefix)]
        assert hits,(method["symbol"],prefix,method["names"])
        for off,name in hits:
            target=branch_target(method["va"]+off,word_be(method["linked"],off))
            rows=[r for r in image.map_rows if r["symbol"]==name]
            assert any(r["va"]==target for r in rows),(method["symbol"],name,hex(target),rows[:3])
            owner=next(r["owner"] for r in rows if r["va"]==target)
            result.append(dict(offset=hex(off),site=hex(method["va"]+off),
                               symbol=name,target_va=hex(target),target_object=owner))
    return result

def verify_methods(image,kdefault_va):
    asm_actual={}
    asm_pins={}
    expected=defaultdict(set)
    methods=[]
    for spec in METHODS:
        obj=image.selected_object(spec["source"])
        asm=obj.with_suffix(".asm")
        assert asm.is_file(),asm
        asm_text=asm.read_text(encoding="utf-8",errors="replace")
        offsets=[int(m.group(1),16) for m in re.finditer(
            r"(?m)^\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]{8}\s+lwz\s+[^\r\n]*K_DefaultSendName",
            asm_text)]
        if spec["source"] in asm_actual:
            assert asm_actual[spec["source"]]==offsets,(spec["source"],asm_actual[spec["source"]],offsets)
        else:
            asm_actual[spec["source"]]=offsets
            asm_pins[spec["source"]]=dict(path=str(asm),sha256=sha(asm),load_offsets=[hex(x) for x in offsets])
        assert spec["offset"] in offsets,(spec["key"],offsets,spec["offset"])
        expected[spec["source"]].add(spec["offset"])
        method=image.method(spec["source"],spec["prefix"])
        ref=check_data_ref(method,KDEFAULT,kdefault_va,spec["offset"])
        calls=resolve_calls(image,method,spec["calls"]) if spec["calls"] else []
        methods.append(dict(key=spec["key"],source=spec["source"],symbol=method["symbol"],
                            va=hex(method["va"]),object_sha256=method["object_sha256"],
                            artist_sites=spec["artist"],load_offset=hex(spec["offset"]),
                            data_reference=ref,resolved_calls=calls,
                            local_absolute_targets=method["local_absolute_targets"]))
    counts={}
    for source,offsets in asm_actual.items():
        assert set(offsets)==expected[source],(source,offsets,expected[source])
        counts[source]=len(offsets)
    assert sum(counts.values())==12
    return counts,asm_pins,methods
def verify_callers(image):
    receipts=[]
    for spec in CALLERS:
        owner=Path(image.rows[spec["source"]]["object"]).name
        candidates=[r for r in image.map_rows if r["symbol"].startswith(spec["prefix"]) and
                    r["owner"].lower()==owner.lower() and r["segment"]==3]
        hits=[]
        for row in candidates:
            try:
                method=image.method(spec["source"],row["symbol"])
            except AssertionError:
                continue
            if not any(kind==6 and method["names"].get((off,kind),"").startswith(spec["helper"]) for off,_,kind in method["relocs"]): continue
            edges=resolve_calls(image,method,[spec["helper"]])
            hits.append((method,edges))
        assert len(hits)==1,(spec["key"],[x[0]["symbol"] for x in hits])
        method,edges=hits[0]
        receipts.append(dict(key=spec["key"],source=spec["source"],
                             symbol=method["symbol"],va=hex(method["va"]),
                             edges=edges,object_sha256=method["object_sha256"]))
    return receipts

def verify_initializer(image,root_obj,kdefault_va,init_va):
    spec=image.method(E0_PROVIDER,"??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ")
    assert spec["va"]==init_va
    calls=resolve_calls(image,spec,[MAKEHASH])
    assert len(calls)==1
    global_ref=check_data_ref(spec,KDEFAULT,kdefault_va)
    obj_data=Path(spec["object"]).read_bytes()
    data_relocs=defaultdict(dict)
    for off,idx,kind in spec["relocs"]:
        name,_,_=object_symbol(obj_data,idx)
        if kind in (0x10,0x11):
            data_relocs[name][kind]=off
    strings=[]
    for name,pair in data_relocs.items():
        if name.startswith("??_C@") and set(pair)=={0x10,0x11}:
            idx=next(idx for off,idx,kind in spec["relocs"]
                     if kind==0x10 and object_symbol(obj_data,idx)[0]==name)
            literal=symbol_literal(obj_data,idx)
            if literal=="Send01":
                target=data_target(spec["linked"],pair[0x10],pair[0x11])
                raw=image.read(target,len(literal)+1)
                assert raw==b"Send01\0"
                strings.append(dict(symbol=name,high_reloc=hex(pair[0x10]),
                                    low_reloc=hex(pair[0x11]),linked_va=hex(target),
                                    bytes=raw.hex()))
    assert len(strings)==1,strings
    return dict(symbol=spec["symbol"],object_sha256=spec["object_sha256"],
                va=hex(init_va),makehash_call=calls[0],
                global_store_target=global_ref,send01_literal=strings[0])

def verify_crt(image,root_obj,kdefault_va):
    m=re.search(r"(?m)^\s+([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})H\s+\.CRT\$XCU\s+DATA\s*$",
                image.map_text)
    assert m
    seg,start,size=(int(m.group(i),16) for i in (1,2,3))
    k=exact_map(image.map_rows,KDEFAULT,root_obj)
    assert k["segment"]==seg
    seg_base=k["va"]-k["offset"]
    crt_va=seg_base+start
    data=image.read(crt_va,size)
    assert size%4==0
    words=list(struct.unpack(f">{size//4}I",data))
    symbols=[
      ("K_DefaultSendName","??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ"),
      ("gRwacTestBedAlloc","??__EgRwacTestBedAlloc@@YAXXZ"),
      ("gCsisTestBedAlloc","??__EgCsisTestBedAlloc@@YAXXZ"),
      ("gPlaybackTestBedAlloc","??__EgPlaybackTestBedAlloc@@YAXXZ"),
      ("gLogicTestBedAlloc","??__EgLogicTestBedAlloc@@YAXXZ"),
    ]
    records=[]
    for label,symbol in symbols:
        row=exact_map(image.map_rows,symbol,root_obj)
        indexes=[i for i,x in enumerate(words) if x==row["va"]]
        assert len(indexes)==1,(label,hex(row["va"]),indexes)
        records.append(dict(name=label,initializer=symbol,va=hex(row["va"]),crt_index=indexes[0]))
    indexes=[r["crt_index"] for r in records]
    assert indexes==list(range(indexes[0],indexes[0]+len(indexes))),indexes
    assert [r["name"] for r in records]==[
      "K_DefaultSendName","gRwacTestBedAlloc","gCsisTestBedAlloc",
      "gPlaybackTestBedAlloc","gLogicTestBedAlloc"]
    return dict(segment=hex(seg),section_offset=hex(start),crt_va=hex(crt_va),
                size=hex(size),bytes_sha256=hashlib.sha256(data).hexdigest(),
                records=records,final_order=[r["name"] for r in records],contiguous=True,
                inherited_artist_allocator_order=["gRwacTestBedAlloc","gLogicTestBedAlloc",
                                                  "gPlaybackTestBedAlloc","gCsisTestBedAlloc"])

def verify_sources(image):
    assert git("rev-parse",HEAD)==HEAD
    assert git("rev-parse",f"{HEAD}^{{tree}}")==TREE
    pins_doc=load(SOURCE_PINS)
    assert sha(SOURCE_PINS).lower()==SOURCE_PINS_SHA
    e0=[]
    for pin in pins_doc:
        path=pin["path"]
        final=git("rev-parse",f"{HEAD}:{path}")
        donor=git("rev-parse",f"{DONOR}:{path}")
        assert final==donor
        inv=next(x for x in image.sources if
                 x["path"].replace("\\","/").lower().endswith("/"+path.lower()))
        assert inv["sha256"].lower()==pin["working_sha256"].lower()
        selected=path in image.rows
        if path.endswith(".cpp"):
            assert selected
            image.selected_object(path)
        e0.append(dict(path=path,git_blob=final,source_sha256=inv["sha256"],selected=selected))
    e4=[]
    for path in E4_SOURCES:
        final=git("rev-parse",f"{HEAD}:{path}")
        donor=git("rev-parse",f"{DONOR}:{path}")
        assert final==donor
        assert path in image.rows
        image.selected_object(path)
        inv=next(x for x in image.sources if
                 x["path"].replace("\\","/").lower().endswith("/"+path.lower()))
        e4.append(dict(path=path,git_blob=final,source_sha256=inv["sha256"],
                       object_sha256=image.rows[path]["sha256"]))
    return e0,e4

def verify_artist():
    assert sha(ARTIST_REPORT).lower()==ARTIST_REPORT_SHA
    assert sha(SOURCE_PINS).lower()==SOURCE_PINS_SHA
    visible=[]
    for addr,name,expected in ARTIST_E0_EXPORTS:
        path=ARTIST_DIR/(addr+".json")
        j=load(path)
        assert j["address"]==addr and j["name"]==name
        count=len(re.findall(r"(?m)^0x[0-9A-Fa-f]+\s+lwz\s+[^\r\n]*dword_8300A6E0@l",j["assembly"]))
        assert count==expected,(addr,count,expected)
        visible.append(dict(address=addr,function=name,loads=count,sha256=sha(path)))
    assert sum(x["loads"] for x in visible)==14
    e4=[]
    for addr,name,expected in ARTIST_E4_EXPORTS:
        j=load(ARTIST_DIR/(addr+".json"))
        assert j["address"]==addr and j["name"]==name
        count=len(re.findall(r"(?m)^0x[0-9A-Fa-f]+\s+lwz\s+[^\r\n]*dword_8300A6E4@l",j["assembly"]))
        assert count==expected,(addr,count,expected)
        e4.append(dict(address=addr,function=name,loads=count,
                       sha256=sha(ARTIST_DIR/(addr+".json"))))
    suspension_source=load(ARTIST_DIR/"0x826F4268.json")
    suspension_callers=[x for x in suspension_source["xrefs_to"]
                        if x["address"]=="0x826FE010" and "UpdateSuspensionSqueeks" in x["name"]]
    assert len(suspension_callers)==1
    init_e0=load(ARTIST_DIR/"0x82C61A18.json")
    init_e4=load(ARTIST_DIR/"0x82C61F48.json")
    init_e1=load(ARTIST_DIR/"0x82C635F8.json")
    assert all(x in init_e0["assembly"] for x in ("Send01","Playback__Name__MakeHash","dword_8300A6E0"))
    assert all(x in init_e4["assembly"] for x in ("Send01","Playback__Name__MakeHash","dword_8300A6E4"))
    assert all(x in init_e1["assembly"] for x in ("Send01","Playback__Name__MakeHash","dword_8300817C"))
    return dict(report_sha256=sha(ARTIST_REPORT),e0_visible_loads=visible,
                e0_visible_count=14,e0_raw_only_sites=ARTIST_RAW_ONLY,
                e0_total_load_sites=16,e4_reader_loads=e4,e4_total_loads=7,
                e0_initializer=dict(address="0x82C61A18",sha256=sha(ARTIST_DIR/"0x82C61A18.json")),
                inair_suspension_calls_play_landing=suspension_callers[0],
                e4_initializer=dict(address="0x82C61F48",sha256=sha(ARTIST_DIR/"0x82C61F48.json")),
                other_send01_initializer=dict(address="0x82C635F8",sha256=sha(ARTIST_DIR/"0x82C635F8.json")))

def verify_other_send01(image,kdefault_va):
    source=E4_SOURCES[1]
    prefix="??__EguSend01Name@Engines@Vehicles@BrnSound@@YAXXZ"
    method=image.method(source,prefix)
    calls=resolve_calls(image,method,[MAKEHASH])
    obj=Path(method["object"]).read_bytes()
    pairs=defaultdict(dict)
    for off,idx,kind in method["relocs"]:
        name,_,_=object_symbol(obj,idx)
        if kind in (0x10,0x11):
            pairs[name][kind]=off
    targets=[]
    strings=[]
    for name,pair in pairs.items():
        if set(pair)!={0x10,0x11}:
            continue
        idx=next(i for off,i,kind in method["relocs"]
                 if kind==0x10 and object_symbol(obj,i)[0]==name)
        try:
            literal=symbol_literal(obj,idx)
        except Exception:
            literal=""
        va=data_target(method["linked"],pair[0x10],pair[0x11])
        if literal=="Send01":
            raw=image.read(va,len(literal)+1)
            assert raw==b"Send01\0"
            strings.append(dict(symbol=name,va=hex(va),bytes=raw.hex()))
        else:
            targets.append(dict(symbol=name,va=hex(va)))
    assert len(calls)==1 and len(targets)==1 and len(strings)==1
    assert int(targets[0]["va"],16)!=kdefault_va
    return dict(source=source,object=method["object"],object_sha256=method["object_sha256"],
                initializer=method["symbol"],va=hex(method["va"]),
                makehash_call=calls[0],send01_literal=strings[0],
                separate_send_storage=targets[0],same_storage_as_kdefault=False)
def main():
    image=LinkedImage()
    e0_pins,e4_pins=verify_sources(image)
    artist=verify_artist()

    root_obj=Path(image.rows[E0_PROVIDER]["object"]).name
    k=exact_map(image.map_rows,KDEFAULT,root_obj)
    init=exact_map(image.map_rows,"??__EK_DefaultSendName@Playback@CgsSound@@YAXXZ",root_obj)
    assert k["va"]==0x82F95804 and init["va"]==0x82A426A0
    rwac=exact_map(image.map_rows,"?gRwacTestBedAlloc@@3UAllocator@TestBed@CgsSound@@A",root_obj)
    assert rwac["va"]==k["va"]+4
    provider=image.selected_object(E0_PROVIDER)
    provider_asm=provider.with_suffix(".asm").read_text(encoding="utf-8",errors="replace")
    assert provider_asm.count("PUBLIC\t"+KDEFAULT)==1
    assert re.search(r"(?m)^\?K_DefaultSendName@Playback@CgsSound@@3UName@12@B DD 01H DUP \(\?\)",provider_asm)

    other_send=verify_other_send01(image,k["va"])
    crt=verify_crt(image,root_obj,k["va"])
    dual_obj=Path(other_send["object"]).name
    other_init=exact_map(image.map_rows,other_send["initializer"],dual_obj)
    assert other_init["va"]==int(other_send["va"],16)
    m=re.search(r"(?m)^\s+([0-9a-fA-F]{4}):([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})H\s+\.CRT\$XCU\s+DATA\s*$",
                image.map_text)
    seg,start,size=(int(m.group(i),16) for i in (1,2,3))
    k_map=exact_map(image.map_rows,KDEFAULT,root_obj)
    crt_words=struct.unpack(f">{size//4}I",image.read(k_map["va"]-k_map["offset"]+start,size))
    indices=[i for i,value in enumerate(crt_words) if value==other_init["va"]]
    assert len(indices)==1,indices
    crt["separate_guSend01Name_initializer"]=dict(symbol=other_init["symbol"],
        va=hex(other_init["va"]),crt_index=indices[0],object=dual_obj,
        distinct_from_kdefault=True)

    initializer=verify_initializer(image,root_obj,k["va"],init["va"])
    load_counts,asm_pins,methods=verify_methods(image,k["va"])
    helper_callers=verify_callers(image)

    global_asm=Path(image.selected_object(E0_READERS[1])).with_suffix(".asm").read_text(
        encoding="utf-8",errors="replace")
    stream_asm=Path(image.selected_object(E0_READERS[5])).with_suffix(".asm").read_text(
        encoding="utf-8",errors="replace")
    assert "Name::MakeHash" in global_asm and "Send01" in global_asm
    assert re.search(r"(?m)^\s+[0-9a-fA-F]+\s+[0-9a-fA-F]{8}\s+addi\s+r6,r1,luSend01\$",global_asm)
    assert "?SetGain@Voice@Logic@CgsSound@@QAAXIMPBI@Z" in global_asm
    assert re.search(r"(?m)^\s+[0-9a-fA-F]+\s+[0-9a-fA-F]{8}\s+addi\s+r4,r1,liSendName\$",stream_asm)
    assert "?GetGain@VoiceWrapper@Logic@CgsSound@@QBAMPBH@Z" in stream_asm

    report={
      "state":"pass",
      "head":HEAD,
      "tree":TREE,
      "link_pins":PINS,
      "strict_link":{"exit_code":image.result["exit_code"],
                     "all_post_checks_pass":image.result["all_post_checks_pass"],
                     "selected":len(image.signed),"excluded":len(image.seal["excluded"]),
                     "unresolved":image.diagnostics["unresolved_messages"],"duplicates":len(image.diagnostics["duplicate_messages"]),
                     "native_gate_passed":image.native_report["passed"],"native_gate_failed":image.native_report["failed"],
                     "post_link_checks":len(image.post_checks),
                     "diagnostics_sha256":sha(LINK/"diagnostic-parsed.json"),
                     "post_link_checks_sha256":sha(LINK/"post-link-checks.json")},
      "e0_source_pins":e0_pins,
      "artist":artist,
      "provider":{"source":E0_PROVIDER,"object":str(provider),
                  "object_sha256":image.rows[E0_PROVIDER]["sha256"],
                  "global_symbol":KDEFAULT,"global_va":hex(k["va"]),
                  "word_size":4,"rwac_va":hex(rwac["va"]),
                  "initializer":initializer},
      "crt_order":crt,
      "generated_loads_by_source":load_counts,
      "selected_assembly_pins":asm_pins,
      "generated_load_count":sum(load_counts.values()),
      "linked_reader_methods":methods,
      "shared_helper_callers":helper_callers,
      "other_send01":{"e4_source_pins":e4_pins,"dual_ginsu_provider":other_send,
                      "artist_e4_init":"0x82C61F48","artist_e4_readers":artist["e4_reader_loads"],
                      "artist_other_send01_init":"0x82C635F8"},
      "pointer_abi":{
         "connect":"u32 send hash passed by value in r4 to Voice::Connect(u32,u32)",
         "streaming_getgain":"E0 copied to local s32; local address is passed to GetGain(const s32*)",
         "global_setgain":"separate local Send01 word remains passed by address to SetGain(...,const u32*)",
         "name_pointer_passed":False},
      "notes":[
         "The final CRT array places K_DefaultSendName immediately before all four root allocator initializers.",
         "ARTIST allocator order after E0 is Rwac/Logic/Playback/Csis; the inherited candidate order is Rwac/Csis/Playback/Logic.",
         "The final InAir listing has an extra MakeSplicerParams call in UpdateSuspensionSqueeks because PlayLanding is inlined. It reuses one static K_DefaultSendName load; ARTIST's suspension path calls PlayLanding, whose E0 load is already one of the 16 listed sites.",
         "Static link evidence only; no runtime launch or gameplay result."
      ]
    }
    output=HERE/"R157_E0_LINKED.json"
    output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(output,sha(output))

if __name__=="__main__":
    main()


















