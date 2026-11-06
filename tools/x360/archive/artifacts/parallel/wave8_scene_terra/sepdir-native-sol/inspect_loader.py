from pathlib import Path
import ast,inspect
p=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/sepdir-native-sol/validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng' for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
print(inspect.getsource(F.Bundle))
print(inspect.getsource(F.address))
