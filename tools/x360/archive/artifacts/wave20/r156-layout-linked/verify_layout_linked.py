"""Bind R156 layout witnesses to selected COFF and a completed PPC PE."""
import argparse,hashlib,json,struct,subprocess,sys
if not __debug__: raise RuntimeError("verification requires Python assertions enabled")
from pathlib import Path
wave=Path(r"D:/Reverse/BP-X360-Experimental-Artifacts/wave20")
helper=wave/"r128-batch/verify_matches.py"
m={"__file__":str(helper)}
exec(helper.read_text().split("\nif len(sys.argv)==2:",1)[0],m)
base=wave/"r156-composed-preflight"
root=Path(r"C:/Users/adri1/.codex/worktrees/bp360/native-sound")
candidate="8157850d32d5d15c6187b22969e939f5ca7c201d"
private_candidate="9708f3b6910954ea62a0a118405b4f9b23c163c6"
probe_candidate="b521454a05f57df74a009c39c89c47af55877d4d"
candidate_tree="f49d1bf3c086e3a3ac5e5aaae120c1acae04b25a"
baseline_hashes={"traffic":"b2b6ae3ebd4f50b17e036f5349303630bf7c73750522db1ed83898656ec8f20d", "replay":"f790abdd3001ea010747af0dcd8ebe046e01e10f3aeba7a05b568eac632b7d6a", "io":"36138a4e293e3d657265044cdc015616d3edf74f7a2ee4e7eb1f71e9c812f6ba"}
units={
"traffic":("src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.cpp","BrnTrafficEntityModule-31ae7ead.obj"),
"replay":("src/GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiser.cpp","BrnReplayTrafficEntitySerialiser-54b21d31.obj"),
"io":("src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.cpp","BrnTrafficEntityModuleIO-98b485fd.obj")}
funcs={
"traffic_prepare":("traffic","?Prepare@TrafficEntityModule@BrnTraffic@@QAA_NPAVOutputBuffer_Prepare@BrnTrafficIO@2@@Z"),
"collector":("traffic","?UpdateSerialiser@TrafficEntityModule@BrnTraffic@@QAAXXZ"),
"read":("replay","?ReadFrame@TrafficEntitySerialiser@BrnReplays@@QAAXPAUTrafficEntitySerialiserStaticLayout@2@H@Z"),
"write":("replay","?WriteFrame@TrafficEntitySerialiser@BrnReplays@@QAAXPAUTrafficEntitySerialiserStaticLayout@2@H@Z"),
"io_read":("io","?GetReplayRequestInterface@OutputBuffer_PostPhysics@BrnTrafficIO@BrnTraffic@@QBAPBURequestInterface@ReplayIO@BrnReplays@@XZ"),
"io_write":("io","?GetReplayRequestInterface@OutputBuffer_PostPhysics@BrnTrafficIO@BrnTraffic@@QAAPAURequestInterface@ReplayIO@BrnReplays@@XZ")}
m["fixed_equal"].__globals__["masks"][2]=0xffffffff # PPC ADDR32, verified separately against the in-function jump target
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def probe_value(obj, name):
 data=obj.read_bytes()
 machine,count,_,table,n,optional,_=struct.unpack_from("<HHIIIHH",data)
 assert machine==0x1f2 and optional==0
 matches=[]; i=0
 while i<n:
  sym,value,section=m["object_symbol"](data,i)
  storage,aux=struct.unpack_from("<BB",data,table+i*18+16)
  if sym==name and 1<=section<=count and storage==2: matches.append((section,value))
  i+=1+aux
 assert len(matches)==1 and matches[0][1]==0,(name,"ambiguous probe symbol")
 section=matches[0][0]
 _,_,_,size,raw,_,_,nrel,_,_=struct.unpack_from("<8sIIIIIIHHI",data,20+(section-1)*40)
 assert nrel==0,(name,"unexpected probe relocations")
 code=data[raw:raw+size]
 words=[x[0] for x in struct.iter_unpack(">I",code)]
 assert words[-1]==0x4e800020, (name,"missing return")
 if len(words)==2 and words[0] & 0xffff0000 == 0x38600000:
  v=words[0]&0xffff; return v if v<0x8000 else v-0x10000
 if len(words)==3 and words[0] & 0xffff0000 == 0x3c600000 and words[1] & 0xffff0000 == 0x60630000:
  return ((words[0]&0xffff)<<16)|(words[1]&0xffff)
 raise AssertionError((name,"unexpected probe instructions",[hex(w) for w in words]))

def verify(folder,head,tree,seal_hash,xex_hash,pe_hash,map_hash):
 expected={"manager_size":"0x12d0","manager_align":"0x8","light":"0x53790","add_hulls":"0x54a60","physics":"0x58210","bits":"0x713a0","race_state":"0x71480","data":"0x71840","world_align":"0x80","module_align":"0x80","world_traffic":"0x18a80","replay_snapshot":"0x22fb0","replay_size":"0x3d480","light_record":"0x8","light_flags":"0x5","phys_entities":"0x193c0","phys_vehicles":"0x194b4","phys_joints":"0x194c0","phys_used":"0x19868","phys_map":"0x198f0","phys_input_articulated":"0x21590"}
 values=json.loads((Path(__file__).parent/"candidate_layout_values.json").read_text())
 for k,v in expected.items():assert values[k]==v,(k,values[k],v)
 probe=Path(__file__).parent/"candidate_layout_probe.obj"
 assert sha(probe)=="897cd6cc741bfa6bc5875185687d1befd7c8e186b785098dc5234f116ecc5607"
 assert sha(Path(__file__).parent/"candidate_layout_probe.cpp")=="6e9fd7e54c24fe0d635671aa942dcef425af68a44cc64a905336f0e770b236ab"
 for k,v in values.items(): assert probe_value(probe,k)==int(v,16),(k,v)
 assert head==candidate and tree==candidate_tree
 assert subprocess.check_output(["git","-C",str(root),"rev-parse",private_candidate+"^{tree}"]).decode().strip()==candidate_tree
 tree_actual=subprocess.check_output(["git","-C",str(root),"rev-parse",head+"^{tree}"]).decode().strip();assert tree_actual==tree
 inv=json.loads((folder/"invocation.json").read_text());result=json.loads((folder/"result.json").read_text());inventory=json.loads((folder/"source-inventory.json").read_text());assert inventory["head"]==head and result["exit_code"]==0 and result["all_post_checks_pass"]
 hashes={"native-game.exe":xex_hash,"native-game.pe":pe_hash,"native-game.map":map_hash}
 for name,digest in hashes.items():
  path=folder/name;assert sha(path)==digest and result["outputs"][str(path)]["sha256"]==digest
 assert sha(folder/"seal.json")==seal_hash
 image=json.loads((folder/"image-verification.json").read_text());assert image["head"]==head and image["image_sha256"]==xex_hash and image["magic"]=="XEX2"
 address,load=m["linked_reader"](folder); selected={}; rows={}
 for key,(source,baseline) in units.items():
  item=next(x for x in inv["objects"] if x["source"]==source);path=Path(item["object"]);assert sha(path)==item["sha256"]
  reference=(wave/"r156-allocator-v2-preflight"/baseline) if key=="traffic" else (base/baseline)
  selected[key]=(path,reference); assert sha(reference)==baseline_hashes[key]
 for label,(key,symbol) in funcs.items():
  obj,old=selected[key]; code,rels=m["collect"](obj,symbol);oldcode,oldrels=m["collect"](old,symbol)
  m["fixed_equal"](oldcode,code,oldrels)
  def normalized(path,rels):
   data=path.read_bytes();return [(o,k,m["object_symbol"](data,i)[0],m["object_symbol"](data,i)[1] if k==2 else None) for o,i,k in rels]
  selected_rels=normalized(obj,rels); reference_rels=normalized(old,oldrels)
  assert len(selected_rels)==len(reference_rels),(label,"relocation count")
  for actual,reference in zip(selected_rels,reference_rels):
   if actual==reference: continue
   off,kind,name,_=actual; oldoff,oldkind,oldname,_=reference
   assert label=="traffic_prepare" and (off,kind)==(oldoff,oldkind),(label,actual,reference)
   if off in (0xe8,0xf4) and kind in (0x10,0x11):
    assert name=="??_C@_0GP@OMMEHHOH@C?3?2Users?2adri1?2?4codex?2worktrees?2@" and oldname=="??_C@_0GP@KMEAIIBK@C?3?2Users?2adri1?2?4codex?2worktrees?2@"
    index=next(i for o,i,k in rels if o==off and k==kind)
    oldindex=next(i for o,i,k in oldrels if o==off and k==kind)
    suffix=r"\src\GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
    assert m["symbol_literal"](obj.read_bytes(),index)==r"C:\Users\adri1\.codex\worktrees\bp360\wave4-intake"+suffix
    assert m["symbol_literal"](old.read_bytes(),oldindex)==r"C:\Users\adri1\.codex\worktrees\bp360\native-sound"+suffix
   elif off==0x2c4 and kind==6:
    assert name=="?AllocateTrafficDebugResource@?A0xca136ec0@BrnTraffic@@YAPAXII@Z" and oldname=="?AllocateTrafficDebugResource@?A0x74615d97@BrnTraffic@@YAPAXII@Z"
   else: raise AssertionError((label,actual,reference))
  import re
  mapping=(folder/"native-game.map").read_text(errors="replace")
  present=bool(re.search(r"(?m)^\s+[0-9a-fA-F]{4}:[0-9a-fA-F]+\s+"+re.escape(symbol)+r"\s+[0-9a-fA-F]{8}\s",mapping))
  if label=="traffic_prepare": assert present, "live TrafficEntityModule::Prepare absent from map"
  va=address(symbol) if present else None
  linked=load(va,len(code)) if present else None
  if present:m["fixed_equal"](code,linked,rels)
  branches={}
  if present:
   for off,idx,kind in rels:
    if kind==2:
     name,value,section=m["object_symbol"](obj.read_bytes(),idx)
     data=obj.read_bytes(); _,_,_,table,count,_,_=struct.unpack_from("<HHIIIHH",data)
     matches=[]; si=0
     while si<count:
      sname,_,ssection=m["object_symbol"](data,si)
      if sname==symbol: matches.append(ssection)
      si+=1+data[table+si*18+17]
     assert len(matches)==1 and section==matches[0] and name.startswith("$LN") and 0<=value<len(code),(label,name,"nonlocal ADDR32")
     assert struct.unpack_from(">I",linked,off)[0]==va+value+struct.unpack_from(">I",code,off)[0],(label,name,"ADDR32 target")
     continue
    if kind!=6:continue
    name=m["object_symbol"](obj.read_bytes(),idx)[0];target=m["branch_target"](va+off,struct.unpack_from(">I",linked,off)[0]);assert target==address(name),(label,name);branches[hex(off)]={"name":name,"va":hex(target)}
  if label=="traffic_prepare":
   assert bytes.fromhex("616b13a0") in code and bytes.fromhex("3a101840") in code
  if label in ("io_read","io_write"):
   assert bytes.fromhex("3c7f000d3863bce0") in code, (label,"request interface must be +0xcbce0")
  if label=="read":
   assert all(bytes.fromhex(x) in code for x in ("3be00258","3bbd2fb0","3bdd0005","3bde0008","389d12c4","997d12c0"))
  if label=="write":
   assert all(bytes.fromhex(x) in code for x in ("3be00258","3bbd2fb0","3bdd0005","3bde0008","389d12c0","389d12c4"))
  if label=="collector":
   sequence=bytes.fromhex("3c9f00053c79000238a012d03884379038632fb0");where=code.find(sequence);assert where>=0
   assert any(o==where+20 and k==6 and m["object_symbol"](obj.read_bytes(),i)[0]=="memcpy" for o,i,k in rels)
   if present:assert branches[hex(where+20)]["name"]=="memcpy"
  rows[label]={"symbol":symbol,"state":"linked" if present else "selected-coff-only-absent-from-map", "va":hex(va) if present else None,"bytes":len(code),"selected_object":str(obj),"selected_sha256":sha(obj),"core_object_sha256":sha(old),"branches":branches}
 headers=["src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h","src/GameSource/Replays/Serialisers/BrnReplayTrafficEntitySerialiserFrame.h","src/GameSource/Replays/Serialisers/BrnReplayTrafficSerialiserFrame.h","src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h","src/GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h","src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h","src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleInputInterface.h"]
 pins={}
 for path in headers+[v[0] for v in units.values()]:
  old=subprocess.check_output(["git","-C",str(root),"rev-parse",(probe_candidate if path in headers else private_candidate)+":"+path]).decode().strip();now=subprocess.check_output(["git","-C",str(root),"rev-parse",head+":"+path]).decode().strip();assert old==now,(path,old,now)
  raw=subprocess.check_output(["git","-C",str(root),"show",head+":"+path]);digest=hashlib.sha256(raw).hexdigest();matches=[x for x in inventory["files"] if x["path"].replace("\\","/").lower().endswith(path.lower())];assert len(matches)==1
  checkout=Path(matches[0]["path"]).read_bytes(); checkout_hash=hashlib.sha256(checkout).hexdigest()
  assert checkout_hash==matches[0]["sha256"] and checkout.replace(b"\r\n",b"\n")==raw,(path,"inventory or content mismatch")
  pins[path]={"git_blob":now,"git_sha256":digest,"inventory_checkout_sha256":checkout_hash,"normalization":"CRLF to LF only"}
 return {"state":"pass","head":head,"tree":tree,"seal_sha256":seal_hash,"image_sha256":hashes,"header_pins":pins,"functions":rows,"candidate":candidate,"private_candidate":private_candidate,"probe_candidate":probe_candidate,"candidate_tree":candidate_tree,"probe_object_sha256":sha(probe),"preflight_report_sha256":sha(base/"compile-results.json"),"traffic_v2_preflight_report_sha256":sha(wave/"r156-allocator-v2-preflight/compile-results.json"),"layout_probe_sha256":sha(Path(__file__).parent/"candidate_layout_values.json"),"scope":"Frozen-source inventory, probe COFF and selected witnesses; PE binding only where symbols appear in map; absent symbols may be discarded or folded. Probe compilation provenance is not independently sealed; not runtime proof"}
if __name__=="__main__":
 a=argparse.ArgumentParser();a.add_argument("link",type=Path);a.add_argument("--head",required=True);a.add_argument("--tree",required=True);a.add_argument("--seal",required=True);a.add_argument("--xex",required=True);a.add_argument("--pe",required=True);a.add_argument("--map",required=True);args=a.parse_args();out=Path(__file__).parent/"R156_LAYOUT_LINKED.json";r=verify(args.link.resolve(),args.head,args.tree,args.seal,args.xex,args.pe,args.map)
 with out.open("x",encoding="utf-8") as f:json.dump(r,f,indent=2);f.write("\n")
 print(out,sha(out))
