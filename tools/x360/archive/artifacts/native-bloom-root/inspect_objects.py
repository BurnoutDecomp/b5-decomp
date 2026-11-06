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
path=Path(report['results'][0]['object'])
for prefix in ['?Construct@BrnPostFxBloom@','?PrepareDownSampleBuffer@','?Generate1PassBlurredBloomBuffer@','?Generate2PassBlurredBloomBuffer@','?Render@BrnPostFxBloom@']:
 product=PackedCoff(path,prefix);o=PackedBundle([product]);o.select(prefix);print(product.fn);print(o.unresolved)
 print('shader data symbols',[(n,hex(o.bases[(0,sec)]+v))for n,v,sec in product.symbols.values()if 'gacBloom' in n and (0,sec) in o.bases])
