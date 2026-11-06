from pathlib import Path
import sys,json,hashlib,struct,re
root=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(root/'tools/re'))
import x360rd
out=Path(__file__).parent
assert x360rd.rd(0x82815570,4).hex()=='7c8a2378'
assets=[]
for name,address,size in [('Arrow',0x82CDC078,0x4000),('Car',0x82CE0078,0x200000),('Text',0x82EE0078,0x40000),('Box',0x82F20078,0x4000)]:
    path=out/'assets'/('gpLoadingScreenTexture'+name+'.bin')
    path.parent.mkdir(exist_ok=True)
    data=x360rd.rd(address,size);path.write_bytes(data)
    assets.append(dict(symbol='BrnGame::gpLoadingScreenTexture'+name,address=hex(address),size=size,path=str(path),sha256=hashlib.sha256(data).hexdigest(),writable=True))
(out/'assets.json').write_text(json.dumps(assets,indent=2)+'\n')
exports=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
funcs={}
addresses=[0x823C6B08,0x823CE208,0x823AAD48,0x823E79C8,0x823E8750,0x823EDE18,0x823EDEB8,0x823C4CB8,0x823FF848,0x82B60B98,0x82B60D28,0x82B60D80,0x82B60E68,0x82B60F38,0x82B60FA8,0x82B61000,0x82B61088,0x82B610D8,0x82B629D8,0x82B62B20,0x82B62CB8,0x82B62D50,0x82B62E18,0x82B62E78,0x82451560,0x82445B28]
for address in addresses:
    j=json.loads((exports/f'0x{address:x}.json').read_text(encoding='utf-8'))
    size=max(int(s,16) for s in re.findall(r'^0x([A-Fa-f0-9]+)',j['assembly'],re.M))+4-address
    raw=x360rd.rd(address,size)
    (out/f'{address:x}.bin').write_bytes(raw)
    (out/f'{address:x}.asm').write_text(j['assembly'],encoding='utf-8')
    funcs[hex(address)]=dict(name=j['name'],size=size,sha256=hashlib.sha256(raw).hexdigest())
(out/'functions.json').write_text(json.dumps(funcs,indent=2)+'\n')
constants={hex(a):x360rd.f32(a) for a in list(range(0x82F24078,0x82F24124,4))+list(range(0x82FAE970,0x82FAE988,4))}
(out/'constants.json').write_text(json.dumps(constants,indent=2)+'\n')
print(json.dumps(assets,indent=2))
print(json.dumps(constants,indent=2))
