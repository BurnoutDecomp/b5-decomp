from pathlib import Path
import ast,inspect,json,struct
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent
row=json.loads((OUT/'candidate-v1/compile-results.json').read_text())['results'][0]
objects=[PackedCoff(Path(row['object']),'?GetMaximumFeature@GPBox@'),PackedCoff(OUT.parent/'volume-baseline/FeatureEdge-wip-v4.obj','??0FeatureEdge@')]
bundle=PackedBundle(objects);bundle.select('?GetMaximumFeature@GPBox@')
print('unresolved',bundle.unresolved);print('epsilon',[(n,v)for n,v in bundle.definitions.items()if 'EPSILON'in n]);print(inspect.getsource(F.M.new_cpu))
c=cpu(bundle);print('sp',hex(c.g(1)));print('memory',[(hex(x[0]),len(x[1]))for x in c.mem.blocks][-8:])
