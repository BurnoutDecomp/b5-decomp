from pathlib import Path
import ast, json, inspect
OUT=Path(__file__).resolve().parent
source=OUT/'validate_construct.py'; nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in n.targets): break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
rows={Path(r['source']).name:r for r in json.loads((OUT/'name-void-v1/compile-results.json').read_text())['results'] if r['passed']}
objects=[PackedCoff(Path(rows[n]['object']),p) for n,p in [('BrnReplayBaseSerialiser.cpp','?Construct@BaseSerialiser@'),('CgsStrStream.cpp','??0StrStream@')]]
bundle=PackedBundle(objects);bundle.select('?Construct@BaseSerialiser@')
print('unresolved',json.dumps({hex(k):v for k,v in bundle.unresolved.items()},indent=2))
print(inspect.getsource(cpu))
print(inspect.getsource(F.Bundle))
