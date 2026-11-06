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
report=json.loads((HERE/'native-candidate-1/compile-results.json').read_text(encoding='utf-8'))
for row in report['results']:
 obj=PackedCoff(Path(row['object']),'?SetShaderConstantArrayData@ShaderConstantTable@@QAAXIPBVVector3Plus@vpu@math@rw@@@Z' if 'Table' in row['source'] else '?ShouldSerialise@')
 print(Path(row['source']).name)
 print('symbols',[(n,v,sec)for n,v,sec in obj.symbols.values()if n.startswith(('?SetShaderConstantArrayData@','?ShouldSerialise@'))])
 print('unresolved',obj.unresolved)
for a in [0x822B34A8,0x822B34F4,0x827E9A18]:
 w=F.M.V.x360rd.u32(a);li=w&0x03FFFFFC;li=li-(1<<26)if li&(1<<25)else li
 print('originalcall',hex(a),hex((a+li)&0xFFFFFFFF))
