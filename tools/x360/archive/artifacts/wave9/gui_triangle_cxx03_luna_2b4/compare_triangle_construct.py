import ast, hashlib, importlib.util, json, re, struct, sys
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
repo=Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\dispatcher_commands_native_luna_2b4')
art=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\gui_triangle_cxx03_luna_2b4')
exp=Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
compile_report=json.loads((art/'signature-compile'/'compile-results.json').read_text(encoding='utf-8'))
row=next(x for x in compile_report['results'] if x['source']=='src/GameShared/GameClasses/Geometric/Intersection/CgsTriangleBox.cpp' and x['passed'])
obj=Path(row['object'])
reader_path=Path(r'D:\Reverse\BP-Decomp_Workflow\tools\re\x360rd.py')
spec=importlib.util.spec_from_file_location('x360rd_triangle_construct',reader_path)
XR=importlib.util.module_from_spec(spec);spec.loader.exec_module(XR)
assert XR.u32(0x82815570)==0x7C8A2378
assert XR.u32(0x82F264A8)==4

checker=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\validate_render_states.py')
text=checker.read_text(encoding='utf-8');tree=ast.parse(text);ns={'M':M,'struct':struct,'re':re}
for node in tree.body:
    if isinstance(node,ast.ClassDef) and node.name in ('CPU','Coff'):
        code=ast.get_source_segment(text,node)
        if node.name=='Coff':
            code=code.replace('def __init__(self, path, prefix):',
                               'def __init__(self, path, prefix, load_base=0x83800000):\n        self.load_base = load_base')
            code=code.replace('0x83800000 + i*0x10000','self.load_base + i*0x10000')
        exec(compile(code,str(checker),'exec'),ns)
Coff=ns['Coff']
coff=Coff(obj,'?Construct@TriangleData@BT@CgsGeometric@@QAAXVVector3@vpu@math@rw@@00@Z')

# Full original ARTIST body at 0x82839D30..0x82839EBC, including its native
# vector-argument ABI (v1/v2/v3) and output stores; no entry normalization.
ORIGINAL=0x82839D30
ORIGINAL_CODE=XR.rd(ORIGINAL,0x190)
THIS=0x86100000
cases=[
 ((0,0,0,0),(1,0,0,0),(0,1,0,0)),
 ((0,0,0,0),(0,1,0,0),(0,0,1,0)),
 ((0,0,0,0),(0,0,1,0),(1,0,0,0)),
 ((1,2,3,0),(1,3,3,0),(2,2,3,0)),
 ((-2,1,4,0),(-1,1,4,0),(-2,1,5,0)),
 ((5,-3,7,0),(5,-2,7,0),(5,-3,8,0)),
]
rows=[]
for case_index,vectors in enumerate(cases):
    original=M.new_cpu();original.mem.add(ORIGINAL,len(ORIGINAL_CODE),ORIGINAL_CODE);original.mem.add(THIS,0xB0,bytearray(0xB0))
    compiled=coff.cpu();compiled.mem.add(THIS,0xB0,bytearray(0xB0))
    original.setg(3,THIS);compiled.setg(3,THIS)
    for reg,values in zip((1,2,3),vectors):
        original.setvf(reg,list(values));compiled.setvf(reg,list(values))
    original.run(ORIGINAL);compiled.run(coff.entry)
    a=original.mem.rd(THIS,0xB0);b=compiled.mem.rd(THIS,0xB0)
    rows.append({'case':case_index,'equal':a==b,'original_sha256':hashlib.sha256(a).hexdigest(),
                 'compiled_sha256':hashlib.sha256(b).hexdigest(),'original_words':a.hex() if a!=b else None,
                 'compiled_words':b.hex() if a!=b else None})
report={'original':'ARTIST CgsGeometric::BT::TriangleData::Construct @0x82839D30 through return at 0x82839EBC',
        'compiled_symbol':coff.fn[0],'compiled_object':str(obj),
        'compiled_object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),
        'raw_reader_sentinels':{'0x82815570':'0x7C8A2378','0x82F264A8':'0x00000004'},
        'input_abi':'this in r3; by-value Vector3 inputs in v1, v2 and v3, matching ARTIST ClipTriWithAABB setup at 0x82840238..250; no vector entry normalization.',
        'cases':len(rows),'passed':sum(r['equal'] for r in rows),'failed':sum(not r['equal'] for r in rows),'mismatches':[r for r in rows if not r['equal']],
        'limits':'Six right-angle unit triangles with finite integer coordinates, including translated and axis-permuted cases; exact full 0xB0 TriangleData output bytes compared between original ARTIST instructions and actual compiled native object. Does not cover degenerate/non-unit normalization cases or full ClipTriWithAABB/IntersectTriangleBox/runtime.'}
out=art/'triangle_construct-original-vs-native-object.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':len(rows),'passed':report['passed'],'failed':report['failed'],'report':str(out)},indent=2))
assert not report['mismatches']
