from pathlib import Path
import ast,json
HERE=Path(__file__).resolve().parent
source=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text(encoding='utf-8')).body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in node.targets):break
 nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report=json.loads((HERE/'native-candidate-1/compile-results.json').read_text(encoding='utf-8'))
r=json.loads((HERE/'native-final/compile-results.json').read_text(encoding='utf-8'))
paths={Path(row['source']).name:Path(row['object'])for row in r['results']}
for name,prefix in [('BrnSoundWorldScene.cpp','?QuerySoundMap@'),('BrnEmitterStateManager.cpp','?GetFreeState@EmitterStateManager@'),('BrnStaticPassbyControl.cpp','?Record@PassbyHistory@')]:
 obj=PackedCoff(paths[name],prefix);print(name,obj.fn);print(obj.unresolved)
