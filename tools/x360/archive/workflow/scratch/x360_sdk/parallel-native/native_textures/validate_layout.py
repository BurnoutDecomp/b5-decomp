from pathlib import Path
import sys,json,struct,hashlib
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'))
from validate_render_states import Coff
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
j=json.loads((out/'compile-results.json').read_text(encoding='utf-8'));p=Path(next(r['object'] for r in j['results'] if r['source'].endswith('native_texture_layout.cpp')));o=Coff(p,'NativeTextureLayout')
name,offset,section=o.fn;s=o.sections[section]
names=['Texture.size','Texture2D.size','Locked.size','Texture.Parameters.size','Texture2D.Parameters.size','Texture.Format','Loading.size','Loading.visible','Loading.time','Loading.rotation','Loading.fade','Loading.background','Loading.overlayFade','Loading.killOverlay','Loading.allocator','Loading.timeStep','Loading.interp','Network.size','Network.default','Network.locked','Network.compressed','Network.lockedCompressed','Network.yuy2','Network.lockedYuy2','Network.flags','Network.prepare','Network.allocator','Network.renderCursor','Network.useDefault']
expected=[52,52,28,28,20,28,96,20,24,32,44,48,52,56,60,88,92,752,8,48,300,312,396,432,684,712,724,736,749]
actual=struct.unpack_from('>'+str(len(expected))+'I',o.data,s[4]+offset)
rows=[dict(name=n,expected=e,actual=a,match=e==a) for n,e,a in zip(names,expected,actual)]
result=dict(object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),records=rows,all_match=all(r['match'] for r in rows),evidence='Native XDK COFF big-endian constants versus SDK header shape and ARTIST loading/network member accesses')
(out/'abi-layout.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(result['all_match'],len(rows));print([r for r in rows if not r['match']]);assert result['all_match']
