from pathlib import Path
import ast,hashlib,json,random,re,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'));import native_atomic_fixture as F;import x360rd as R
folder=sys.argv[1] if len(sys.argv)>1 else 'aspect-correction2';out=parent/'b5-decomp/build/x360/parallel/native_textures'/folder
paths={Path(r['source']).name:Path(r['object']) for r in json.loads((out/'compile-results.json').read_text())['results']}
bundle=F.Bundle([F.Coff(paths['CgsIm2dTransform.cpp'],'?TransformByAspectRatio@'),F.Coff(paths['RwVector3TemplateFloat.cpp'],'??0?$Vector3Template@M')]);bundle.select('?TransformByAspectRatio@')
tree=ast.parse((parent/'scratch/x360_sdk/parallel-native/native_textures/renderer/validate_aspect_correction.py').read_text());nodes=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ['branch','bind']];exec(compile(ast.Module(body=nodes,type_ignores=[]),'aspect-fixture-helpers','exec'));save25=branch(0x827f98cc);rest25=branch(0x827f9b94);bind()
SELF=F.M.S.SELF;rng=random.Random(0x823db448);errors=[]
for i in range(128):
 matrix=struct.pack('>9f',*(rng.randint(-40,40)*0.125 for n in range(9)));image=struct.pack('>16f',*(rng.randint(-40,40)*0.125 for n in range(16)));both=[]
 for compiled in [False,True]:
  c=F.cpu(bundle);c.mem.add(0x83011000,0x1000);c.mem.wr(0x830110f0,matrix);c.mem.wr(SELF,image);c.setg(3,SELF);c.run(bundle.entry if compiled else 0x823db448);both.append(c.mem.rd(SELF,64))
 if both[0]!=both[1]:errors.append(dict(case=i,differences=[hex(n) for n,(a,b) in enumerate(zip(*both)) if a!=b]))
result=dict(cases=128,errors=errors,object=str(paths['CgsIm2dTransform.cpp']),scope='Whole actual TransformByAspectRatio versus ARTIST823DB448 with actual compiled FPU constructors and math. All64transformbytes compare across finite nontrivial basis/matrix inputs.',limits='No shared VPU type changes; native CRT relocations use original save/rest FPR helpers.')
(out/'aspect-in-place-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('128 in-place aspect cases; errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
