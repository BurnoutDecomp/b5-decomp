"""PVS selected PPC comparison and ARTIST error-path witnesses."""
import hashlib,json,pathlib,runpy,struct,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd
wave=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20');out=pathlib.Path(__file__).resolve().parent;m=runpy.run_path(str(wave/'r156-layout-linked/verify_layout_linked.py'))['m']; inv=json.loads((wave/'r165-link/invocation.json').read_text()); item,=[x for x in inv['objects'] if x['source'].endswith('/PVSModule/BrnPVSModule.cpp')];old=pathlib.Path(item['object']);new=out/'BrnPVSModule-r166.obj';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert sha(old)==item['sha256']=='fa16370cde4e02b199af98fd1df20cff3fa3c72bf80d1f50a3040b5b936293e7';assert json.loads((out/'compile-results.json').read_text())['returncode']==0
source=r'd:\p4\b5_main\burnout\main\code\gamesource\unity\../World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.cpp'
functions={'Prepare':('?Prepare@PVSModule@BrnWorld@@UAA_NXZ',{0x16c:('38a000a9','38a0009b'),0x200:('38a000c7','38a000c8')}),'Release':('?Release@PVSModule@BrnWorld@@UAA_NXZ',{0x20:('2f0b0002','2b0b0002'),0x50:('38a000e1','38a00101')})}
for name,(symbol,changes) in functions.items():
 a,ar=m['collect'](old,symbol);b,br=m['collect'](new,symbol);assert len(a)==len(b) and len(ar)==len(br)
 diffs={i:(a[i:i+4].hex(),b[i:i+4].hex()) for i in range(0,len(a),4) if a[i:i+4]!=b[i:i+4]};assert diffs==changes,(name,diffs)
 for (o,i,k),(o2,j,k2) in zip(ar,br):
  assert (o,k)==(o2,k2);an,av,asec=m['object_symbol'](old.read_bytes(),i);bn,bv,bsec=m['object_symbol'](new.read_bytes(),j)
  if an==bn:continue
  if an.startswith('$LN') and bn.startswith('$LN'): assert av==bv and asec>0 and bsec>0;continue
  assert k in (16,17) and ((name=='Prepare' and o in (0x38,0x40,0x140,0x144,0x1fc,0x204)) or (name=='Release' and o in (0x4c,0x54))),(name,hex(o),an,bn)
  if o in (0x38,0x40):
   assert m['symbol_literal'](new.read_bytes(),j).endswith('GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h')
  else:assert m['symbol_literal'](new.read_bytes(),j)==source
for name,offset,message in [('Prepare',0x138,'Invalid event received\n'),('Prepare',0x208,'Invalid Stage\n'),('Release',0x58,'Invalid Stage\n')]:
 c,r=m['collect'](new,functions[name][0]);i,=[i for o,i,k in r if o==offset and k==16];assert m['symbol_literal'](new.read_bytes(),i)==message
c,r=m['collect'](new,functions['Release'][0]);assert c[0x20:0x28]==bytes.fromhex('2b0b000240980020') and c[0x44:0x48]==bytes.fromhex('4099002c') and c[0x68:0x70]==bytes.fromhex('3860000048000014')
assert x360rd.rd(0x822a8af8,4)==bytes.fromhex('2b0b0001') and x360rd.rd(0x822a8b04,4)==bytes.fromhex('2b0b0003')
assert x360rd.rd(0x82302f64,4)==bytes.fromhex('38a0009b') and x360rd.rd(0x82303010,4)==bytes.fromhex('38a000c8') and x360rd.rd(0x822a8b14,4)==bytes.fromhex('38a00101')
r={'state':'pass','base':'451d95889337e064441dec06a1726179187e48cf','object_sha256':sha(new),'baseline_object_sha256':sha(old),'compile_report_sha256':sha(out/'compile-results.json'),'scope':'PPC COFF only; Prepare/Release exact prior fixed words except two line immediates and Release unsigned compare and line immediate; named relocation changes restricted to specified assert and checkout paths; no link/runtime','artist_assert_lines':[155,200,257],'release_stage':'unsigned: 0/1 base release, 2 done, >=3 assert and false'}
(out/'PROOF.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS',sha(out/'PROOF.json'))
