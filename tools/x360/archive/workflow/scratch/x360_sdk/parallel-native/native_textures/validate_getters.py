from pathlib import Path
import sys,json,struct,hashlib,random
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
report=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
row=next(r for r in report['results'] if r['source'].endswith('/texture.cpp') and r['passed'])
obj=F.Coff(Path(row['object']),'?GetType@Texture@')
bundle=F.Bundle([obj]); rng=random.Random(0x360B6100);failures=[];groups=[]
for name,original in [('GetType',0x82B60E68),('GetWidth',0x82B60EC8),('GetHeight',0x82B60F38),('GetDepth',0x82B60FA8),('GetFormat',0x82B61000)]:
    bundle.select('?'+name+'@Texture@')
    for i in range(256):
        header=rng.randbytes(52); results=[]
        for compiled in (False,True):
            c=F.cpu(bundle);c.mem.wr(F.M.S.SELF,header);c.setg(3,F.M.S.SELF)
            c.run(bundle.entry if compiled else original)
            results.append((c.g(3)&0xffffffff,c.mem.rd(F.M.S.SELF,52).hex()))
        if results[0]!=results[1]:failures.append(dict(function=name,index=i,results=results,header=header.hex()))
    groups.append(dict(function=name,cases=256))
    print(name,256,'checked',flush=True)
result=dict(groups=groups,cases=sum(g['cases'] for g in groups),failures=failures,object=row['object'],sha256=hashlib.sha256(Path(row['object']).read_bytes()).hexdigest(),scope='actual original and compiled native getter instruction streams; random native header bits; complete header and return comparison; no GPU execution')
(out/'getter-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('failures',len(failures)); print(failures[:2]); raise SystemExit(bool(failures))
