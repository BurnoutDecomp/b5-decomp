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
import inspect
for cl in TintCPU.__mro__:
 try:code=inspect.getsource(cl)
 except (OSError,TypeError):continue
 for token in ['# lfsx','def f(','def setf(','def fp(','self.fregs']:
  start=code.find(token)
  if start>=0:print(cl.__name__,token,code[max(0,start-160):start+600])
