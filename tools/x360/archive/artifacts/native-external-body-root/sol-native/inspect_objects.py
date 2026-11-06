from pathlib import Path
import ast,inspect,json
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent;rows=json.loads((OUT/'candidate-v2/compile-results.json').read_text())['results']
objects=[PackedCoff(Path(rows[0]['object']),'?CalculateNewVelocity@'),PackedCoff(Path(rows[1]['object']),'??0VecFloat@vpu@math@rw@@QAA@ABM@Z')]
bundle=PackedBundle(objects)
for prefix in ['?CalculateNewVelocity@','?CalculateCollisionImpulseWithBody@','?DampenAngularVelocity@','?IntegrateTransform@']:
    bundle.select(prefix);print(prefix,'unresolved',bundle.unresolved)
print('initializers',[(n,v)for n,v in bundle.definitions.items()if '__E' in n and 'Sixty'in n]);print('sixty symbols',[(n,v,s)for n,v,s in objects[0].symbols.values()if 'Sixty'in n])
