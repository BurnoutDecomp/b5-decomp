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
for address in [0x822B3FA4,0x822B3FFC,0x822B4058,0x822B4078,0x822FF5D0,0x822FF610]:
 w=F.M.V.x360rd.u32(address);li=w&0x03FFFFFC;li=li-(1<<26)if li&(1<<25)else li
 print(hex(address),hex((address+li)&0xFFFFFFFF))
