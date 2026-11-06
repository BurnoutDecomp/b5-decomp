from pathlib import Path
import ast,hashlib,json,struct,subprocess,sys
d=Path(__file__).resolve().parent
root=d.parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='8334855e9506225c53536a484075eba10e12f2e2'
p='src/GameSource/Resource/BrnDLCManager.cpp'
symbol='?Release@DLCManager@BrnResource@@QAA_NXZ'
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
def pin(p):return {'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
assert git('rev-parse','HEAD')==base and git('diff','--name-only')==p
gate=d/'native/compile-results.json'
report=json.loads(gate.read_text())
assert report['requested']==report['passed']==2 and report['failed']==0
assert all(r['machine']==0x1f2 for r in report['results'])
rows={Path(row['source']).stem:row for row in report['results']}
src=Path(rows['BrnDLCManager']['object'])
caller=Path(rows['BrnGameDataModule']['assembly']).read_text(errors='replace')
assert 'bl           '+symbol in caller
verifier=root/'r26-intake-repaired/verify_netconn.py'
nodes=[node for node in ast.parse(verifier.read_text()).body if isinstance(node,ast.FunctionDef) and node.name in ('coff','find')]
ns={'struct':struct}
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(verifier),'exec'),ns)
syms,sections=ns['coff'](src.read_bytes())
(code,rels),offset=ns['find'](symbol,syms,sections)
assert offset==0 and len(code)==120
types={o:k for o,k,s in rels if k!=18}
pairs=[o for o,k,s in rels if k==18]
expected={0x20:6,0x24:16,0x2c:17,0x30:16,0x34:17,0x38:6,0x3c:6}
assert types==expected and pairs==[0x24,0x2c,0x30,0x34],rels
assert {s for o,k,s in rels if k==6}=={'?BeginAssert@Assert@CgsDev@@YAXXZ','?FireAssert@Assert@CgsDev@@YAXPBD0H@Z','?EndAssert@Assert@CgsDev@@YAXXZ'}
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32
artist=rd(0x82662DA0,len(code))
matches=0
for i in range(0,len(code),4):
    a,b=struct.unpack_from('>I',artist,i)[0],struct.unpack_from('>I',code,i)[0]
    if i not in types:
        assert a==b,(hex(i),hex(a),hex(b))
        matches+=1
    else:
        mask=0xfc000003 if types[i]==6 else 0xffff0000
        assert a&mask==b&mask,(hex(i),hex(a),hex(b))
for high,low,string in ((0x24,0x2c,rb'..\..\..\GameSource\Resource/BrnDLCManager.cpp'),
                        (0x30,0x34,b'Invalid stage')):
    addr=((u32(0x82662DA0+high)&0xffff)<<16)+(u32(0x82662DA0+low)&0xffff)
    assert rd(addr,len(string)+1)==string+b'\0'
msg=d/'commit-message.txt'
msg.write_text('Restore native DLC manager release stages\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',p)
git('diff','--cached','--check')
git('commit','--file',str(msg))
head=git('rev-parse','HEAD')
assert not git('status','--porcelain=v1')
evidence={'base':base,'commit':head,'path':p,'native_gate':pin(gate),'object':pin(src),
          'artist':{'address':'0x82662DA0','length':len(artist),'bytes_sha256':hashlib.sha256(artist).hexdigest()},
          'instruction_words':len(code)//4,'exact_nonrelocated_words':matches,
          'relocations':[(o,k,s) for o,k,s in rels if k!=18],'pair_offsets':pairs,
          'matching_relocated_opcode_fields':len(types),
          'scope':'Function object bytes compared with ARTIST before relocation; game link and launch remain unverified.'}
(d/'DELIVERY.json').write_text(json.dumps(evidence,indent=2)+'\n')
print(head,matches,len(types),pin(d/'DELIVERY.json'))
