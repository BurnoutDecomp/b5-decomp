import re, json, hashlib, pefile, capstone
from pathlib import Path
base=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-link'); out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-render-linked')
pe=pefile.PE(str(base/'native-game.pe')); data=(base/'native-game.pe').read_bytes(); lines=(base/'native-game.map').read_text(errors='replace').splitlines()
entries=[]
for s in lines:
 m=re.match(r'\s*[0-9a-fA-F]+:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s+(\S+)',s)
 if m: entries.append((int(m[2],16),m[1],m[3]))
entries.sort(); addresses=sorted(set(x[0] for x in entries)); names={}
for a,n,o in entries: names.setdefault(a,[]).append(n)
patterns=['PreDispatchUpdate@TrafficEntityModule','GenerateDispatchLists@TrafficEntityModule','SubmitCoronasForVehicle@TrafficEntityModule','RenderTrafficLightCoronas@TrafficEntityModule','RenderTrafficCar@TrafficEntityModule','GetPhysicsInfo@TrafficEntitySerialiser','GetVehicleTransform@TrafficEntitySerialiser','GetVehicleData@TrafficEntitySerialiser','GetStaticLayout@TrafficEntitySerialiser','ReadAsQuatPos@TrafficEntitySerialiser','SetPhysicsData@TrafficEntitySerialiser','SetupShaderConstantsForInstancing@Model','GetTrafficPhysicsInfoForVehicl@TrafficEntityModule']
cs=capstone.Cs(capstone.CS_ARCH_PPC,capstone.CS_MODE_32|capstone.CS_MODE_BIG_ENDIAN); cs.detail=False; cs.skipdata=True
results=[]
for a,n,o in entries:
 if not any(p in n for p in patterns): continue
 nxt=next((x for x in addresses if x>a),a+4); code=pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,nxt-a); ins=list(cs.disasm(code,a))
 calls=[]; highlights=[]
 for i in ins:
  if i.mnemonic in ('bl','bla'):
   try: t=int(i.op_str,16)
   except: continue
   calls.append({'at':hex(i.address),'target':hex(t),'symbols':names.get(t,[])})
  if re.search(r'(?<![0-9a-f])(0x1010|0xd30|0xfc8|0xfe5|0x530|0x520|0x30)\b',i.op_str): highlights.append(f'{i.address:08x} {i.mnemonic} {i.op_str}')
 results.append({'name':n,'object':o,'address':hex(a),'end':hex(nxt),'instructions':len(ins),'calls':calls,'offset_witnesses':highlights})
(out/'linked-render.json').write_text(json.dumps({'pe_sha256':hashlib.sha256(data).hexdigest(),'map_sha256':hashlib.sha256((base/'native-game.map').read_bytes()).hexdigest(),'functions':results},indent=2))
for f in results:
 print('\n'+f['name'].split('@')[0]+' '+f['address']+' '+f['end'])
 for c in f['calls']:
  if c['symbols']: print(c['at'], c['target'],c['symbols'][0][:110])
 print('offsets', '; '.join(f['offset_witnesses'][:15]))

