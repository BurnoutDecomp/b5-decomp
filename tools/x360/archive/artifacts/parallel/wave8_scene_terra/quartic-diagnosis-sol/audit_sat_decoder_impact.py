from pathlib import Path
import hashlib,json,importlib.util,sys,struct
OUT=Path(__file__).resolve().parent
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');spec=importlib.util.spec_from_file_location('quartic_coff_records',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
Sat=OUT.parent/'sepdir-native-sol';satrow=json.loads((Sat/'candidate-v4/compile-results.json').read_text())['results'][0];drivers=json.loads((Sat/'drivers-v3-composed-diagnostic.json').read_text())
objects=[Path(satrow['object'])]+[Path(s)for s in drivers['objects']if Path(s)!=Path(json.loads((Sat/'candidate-v3/compile-results.json').read_text())['results'][0]['object'])]
def family(w):return w>>26==4 and(w&0x7f3)in[0x403,0x443]
objhits={}
for p in objects:
 r=m.Records(p);found=[]
 for sec,h in r.sections.items():
  if not h[9]&0x20:continue
  for off in range(0,h[3]-3,4):
   w=struct.unpack_from('>I',r.data,h[4]+off)[0]
   if family(w):found.append({'section':sec,'offset':off,'word':hex(w),'true_vd':((w>>21)&31)|(((w>>2)&3)<<5),'legacy_vd':((w>>21)&31)|((w&3)<<5)})
 objhits[str(p)]=found
rawhits=[]
# Every original function used by the SAT proofs, plus the five interval
# providers in the composed graph. The ranges are their real code extents.
ranges=[(0x82bb4038,0x82bb7728),(0x82baa1a0,0x82baa2a0),(0x82ba80c0,0x82ba8118),(0x82bafc10,0x82bafd30),(0x82bba6e0,0x82bba748),(0x82ba9238,0x82ba93a0),(0x82bad938,0x82badab0)]
for lo,hi in ranges:
 for ea in range(lo,hi,4):
  w=x360rd.u32(ea)
  if family(w):rawhits.append({'pc':hex(ea),'word':hex(w)})
report={'SAT_source_sha256':hashlib.sha256((Sat/'SeparatingDirection.final.cpp').read_bytes()).hexdigest(),'objects':{str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in objects},'compiled_opcode_family_hits':objhits,'original_scanned_ranges':[(hex(a),hex(b))for a,b in ranges],'original_opcode_family_hits':rawhits,'scope':'Conservative static instruction-range scan, not a new execution pass. Includes all actual SAT object text and linked interval-provider object text; potential unused shape feature methods also scanned. No private SAT seal rewritten.'}
p=OUT/'SAT-lvlx128-impact-inventory.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('compiled hits',{Path(p).name:len(v)for p,v in objhits.items()},'original hits',rawhits)
