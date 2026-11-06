from pathlib import Path
import runpy,struct,json,hashlib
p=Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/deformation-next');base=p.parent/'scripts';h=runpy.run_path(str(base/'validate_hacky_whole.py'));s=runpy.run_path(str(base/'validate_swept_whole.py'));V=lambda x:struct.pack('>4f',*x);Z=lambda x:V([x]*4);tri=b''.join(map(Z,[0,0,0,1,0,0,0,1,0]))+struct.pack('>4I',0xffffffff,0,0xffffffff,0)+Z(0)+Z(-1)+Z(-1)+Z(-1);cases=[];bad=[]
for z in [.1,-.1,2.,0.]:
 x=struct.pack('>4f',.2,.2,z,.5);a=h['run'](False,x,tri,Z(0));b=h['run'](True,x,tri,Z(0));cases.append({'kind':'hacky','z':z,'mask':[hex(v) for v in a[0]]});
 if a!=b:bad.append({'kind':'hacky','z':z,'original':a,'compiled':b})
for z,d in [(2.,-1.),(.1,1.),(-2.,1.),(2.,1.)]:
 x=struct.pack('>8f',.2,.2,z,.5,0,0,d,3.);a=s['run'](False,x,tri);b=s['run'](True,x,tri);cases.append({'kind':'swept','z':z,'direction':d,'mask':[hex(v) for v in a[0]]});
 if a!=b:bad.append({'kind':'swept','z':z,'direction':d,'original':a,'compiled':b})
assert any(c['mask'][0]=='0xffffffff' for c in cases) and any(c['mask'][0]=='0x0' for c in cases);print('cases',len(cases),'mismatch',len(bad));o={'cases':cases,'mismatches':bad,'scope':'selected face hits/misses, reverse sweeps, zero-distance boundary and mixed valid mask; compares full output rows/guards and mask, real compiled helpers/getters, emulator only','object_sha256':hashlib.sha256((p/'triangle.obj').read_bytes()).hexdigest()};(p/'triangle-targeted-comparison.json').write_text(json.dumps(o,indent=2)+'\n');assert not bad
