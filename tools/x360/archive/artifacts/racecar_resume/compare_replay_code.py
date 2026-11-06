from pathlib import Path
import ast,re,json,struct,hashlib
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');loader=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');tree=ast.parse(loader.read_text());exec(compile(ast.Module(body=[x for x in tree.body if isinstance(x,ast.ClassDef) and x.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
def obj(folder):
 d=json.loads((root/folder/'compile-results.json').read_text());return Path(d['results'][0]['object'])
a,b=obj('typed_replay_frame_baseline'),obj('typed_replay_frame_final');rows=[]
for method in ['Construct','GetStaticLayout','CheckStaticLayoutCleared','Read','Write']:
 prefix='?'+method+'@RaceCarEntitySerialiser@';x,y=Coff(a,prefix),Coff(b,prefix)
 def signature(z):
  sec=z.fn[2];s=z.sections[sec];return z.data[s[4]:s[4]+s[3]],[(at,z.symbols[i][0],kind) for at,i,kind in z.relocations(sec)]
 ba,ra=signature(x);bb,rb=signature(y);rows.append({'method':method,'bytes_equal':ba==bb,'relocations_equal':ra==rb,'size':len(ba)})
r={'baseline_object':str(a),'baseline_sha256':hashlib.sha256(a.read_bytes()).hexdigest(),'typed_object':str(b),'typed_sha256':hashlib.sha256(b.read_bytes()).hexdigest(),'comparisons':rows,'limit':'Compare native emitted code and symbol-resolved relocations for five serialiser methods; this does not prove frame Clear or runtime replay.'};(root/'typed-replay-code-comparison.json').write_text(json.dumps(r,indent=2),encoding='utf8');print(json.dumps(rows,indent=2));assert all(q['bytes_equal'] and q['relocations_equal'] for q in rows)
