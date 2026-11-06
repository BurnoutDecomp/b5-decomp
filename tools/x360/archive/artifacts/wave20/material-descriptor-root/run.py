import hashlib,json,os,pathlib,subprocess,sys
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
HEAD='19bf3f959dc982024c725b277dfacb8ca00d2e0f'
H='src/GameShared/GameClasses/Graphics/Dispatch/materialcrc32.h'
C=H[:-1]+'cpp'
A='src/GameShared/GameClasses/RenderWare/CgsRwMaterialCRC32ResourceType.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32
def git(*a):return subprocess.check_output(['git',*a],cwd=W).decode().strip()
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def replace(p,old,new):
    file=W/p
    b=file.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    s=b.decode().replace('\r\n','\n')
    assert s.count(old)==1
    file.write_bytes(s.replace(old,new).replace('\n',eol).encode())
assert git('rev-parse','HEAD')==HEAD and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert u32(0x827f7878)==0x3920000c and u32(0x827f7888)==0x39200004
assert u32(0x828a88c4)&0x03fffffc==(0x827f7878-0x828a88c4)&0x03fffffc
replace(H,"""    // materialcrc32.h:47 -- the serialised-resource descriptor for this material (X360
    // 0x827F7878). The body computes the packed serialised size and seeds the descriptor; the
    // exact sizing is owned by the engine SDK home (materialcrc32.cpp) and is not reconstructed
    // here -- this declaration is the surface the resource-type handler forwards to.
    rw::BaseResourceDescriptors<5> GetResourceDescriptor();""",
"""    // ARTIST 0x827F7878. The descriptor depends only on the serialized layout.
    static rw::BaseResourceDescriptors<5> GetResourceDescriptor();""")
replace(A,"""    ResourceDescriptor RwMaterialCRC32ResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // X360 0x828A88B0: mr r31,r3 (save sret); bl MaterialCRC32::GetResourceDescriptor; mr r3,r31.
        // The serialised resource IS the MaterialCRC32 object; forward to its descriptor builder.
        MaterialCRC32* lpMaterial = const_cast<MaterialCRC32*>(static_cast<const MaterialCRC32*>(lpResource));
        return lpMaterial->GetResourceDescriptor();
    }""",
"""    ResourceDescriptor RwMaterialCRC32ResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        return MaterialCRC32::GetResourceDescriptor();
    }""")
with (W/C).open('x',encoding='utf-8',newline='\n') as f:
    f.write("""#include "GameShared/GameClasses/Graphics/Dispatch/materialcrc32.h"

// ARTIST 0x827F7878
rw::BaseResourceDescriptors<5> MaterialCRC32::GetResourceDescriptor()
{
    CGS_STATIC_ASSERT((sizeof(MaterialCRC32) == 12), "ARTIST material size");
    CGS_STATIC_ASSERT((__alignof(MaterialCRC32) == 4), "ARTIST material alignment");

    rw::BaseResourceDescriptors<5> lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(MaterialCRC32);
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = __alignof(MaterialCRC32);
    return lDescriptor;
}
""")
git('diff','--check','--',H,A)
args=json.loads((D.parent/'r26-root-providers/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(D/'native')
for p in (C,A):args+=['--source',p]
temp=D/'temp'
temp.mkdir()
(D/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=W,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report=D/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==2 and r['failed']==0
rows={row['source']:row for row in r['results']}
assert all(row['machine']==498 for row in rows.values())
sym='?GetResourceDescriptor@MaterialCRC32@@SA?AU?$BaseResourceDescriptors@$04@rw@@XZ'
source=pathlib.Path(rows[C]['assembly']).read_text(errors='replace')
caller=pathlib.Path(rows[A]['assembly']).read_text(errors='replace')
assert 'Begin code for function: '+sym in source and any(sym in l and ' bl ' in l for l in caller.splitlines())
msg=D/'message.txt'
msg.write_text('Restore the native CRC32 material resource descriptor\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',A,H,C)
assert set(git('diff','--cached','--name-only').splitlines())=={A,H,C}
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
result={'base':HEAD,'commit':git('rev-parse','HEAD'),'paths':{p:sha(W/p) for p in (A,H,C)},
        'artist_descriptor':'0x827F7878..0x827F78C3 uses only hidden result r3, lane zero {12,4}, four lanes {0,1}',
        'artist_wrapper':'0x828A88B0..0x828A88DF forwards hidden result, no resource pointer reads',
        'report':str(report),'report_sha256':sha(report)}
(D/'DELIVERY.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['commit'])

