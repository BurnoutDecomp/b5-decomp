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
for name,prefix in [('BrnRaceCarEntityModule_Range.cpp','?UpdateInAndOutOfRangeCars@'),('BrnRaceCarEntityModule_NearMissTailgate.cpp','?UpdatePowerParking@'),('BrnRCEntityActiveRaceCarOutputInterface.cpp','?AddCarToRace@')]:
 row=next(r for r in report['results']if Path(r['source']).name==name)
 obj=PackedCoff(Path(row['object']),prefix)
 print(name,obj.fn)
 print('UNRESOLVED',[(hex(a),n)for a,n in obj.unresolved.items() if not name.startswith('BrnRaceCarEntityModule_Range')])
 
