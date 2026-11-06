from pathlib import Path
import ast,sys,inspect
p=Path(__file__).with_name('trace_case1.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.ClassDef)and n.name=='StopTrace':break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
for name in ['validate_native_math','atc_update_emu','scalarsincos']:
 mod=sys.modules[name];print(name,mod.__file__)
 cls=next(x for x in CPU.__mro__ if x.__module__==name)
 print(inspect.getsource(cls.exec_vector)[:16000])
