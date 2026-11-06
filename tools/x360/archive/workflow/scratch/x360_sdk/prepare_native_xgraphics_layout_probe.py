"""Compile actual declarations into layout constants to compare with SDK CodeView."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-xgraphics-layout'
OUT.mkdir(exist_ok=True)
types_path=ROOT/'build/x360/xgraphics-inspection/full-types-0x1e1a576-classes.json'
classes=json.loads(types_path.read_text())
selected=['XGRAPHICS::Compiler','XGRAPHICS::Arena','XGRAPHICS::Compiler::MemStat','XGRAPHICS::Compiler::PerfCounter']
rows=[];expressions=[]
for name in selected:
    c=next(c for c in classes if c.get('name')==name)
    rows.append({'class':name,'field':'sizeof','expected':int(c['size'],16)})
    expressions.append(f'sizeof({name})')
    for field in c['fields']:
        rows.append({'class':name,'field':field['name'],'expected':int(field['offset'],16)})
        expressions.append(f'offsetof({name}, {field["name"]})')
source='#include "SDKs/XGraphics/XGraphicsArena.h"\n#include "SDKs/XGraphics/XGraphicsCompiler.h"\n'
source+='extern "C" const u32 gNativeXGraphicsLayout[] = {\n'+',\n'.join('    '+e for e in expressions)+'\n};\n'
(OUT/'probe.cpp').write_bytes(source.replace('\n','\r\n').encode())
(OUT/'expected.json').write_text(json.dumps({'records':rows,'codeview':str(types_path),'codeview_sha256':hashlib.sha256(types_path.read_bytes()).hexdigest()},indent=2)+'\n')
print(len(rows),'original SDK size/field-offset checks generated')
