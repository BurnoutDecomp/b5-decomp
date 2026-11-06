import hashlib,json,os,pathlib,subprocess,sys

OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='6530087646d77f9452e09cff11c2d8eb8292fac4'
PREFIX='src/SDKs/EATech/rwmovie/'
SOURCE=PREFIX+'CLocalHuffman.cpp'
HEADER=PREFIX+'CLocalHuffman.h'
ALT=PREFIX+'CAltTablesEncoder.cpp'
ALT_HDR=PREFIX+'CAltTablesEncoder.h'
ENCODER=PREFIX+'CLocalHuffmanEncoder.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import rd,u32

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def change(path,transform):
    p=WORK/path
    b=p.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    p.write_bytes(transform(b.decode().replace('\r\n','\n')).replace('\n',eol).encode())
def replace(s,old,new):
    assert s.count(old)==1,old
    return s.replace(old,new)
def literal(name,start,data):
    chunks=[', '.join(f'0x{b:02X}' for b in data[i:i+16]) for i in range(0,len(data),16)]
    return (f'    // ARTIST 0x{start:08X}\n    const u8 {name}[] =\n    {{\n'+
            ',\n'.join('        '+row for row in chunks)+'\n    };')

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
assert u32(0x8322c3f0)==0
assert u32(0x82a7ea0c)&0x03fffffc==0x4310c
CODE_START=0x82123db0
CODE_END=0x82123e38+2*73
PACKED_START=0x82f706a8
PACKED_END=0x82f70d90+1+4*32
code=rd(CODE_START,CODE_END-CODE_START)
packed=rd(PACKED_START,PACKED_END-PACKED_START)
modes=[
    ('KPU_SECONDARY_01_LEGACY',0x82f70818,77,3),('KPU_SECONDARY_01_CURRENT',0x82f706d8,77,3),
    ('KPU_SECONDARY_23_LEGACY',0x82f70a98,77,3),('KPU_SECONDARY_23_CURRENT',0x82f70958,77,3),
    ('KPU_SECONDARY_4_LEGACY',0x82f706b8,12,2),('KPU_SECONDARY_4_CURRENT',0x82f706a8,12,1),
    ('KPU_SECONDARY_WIDE_LEGACY',0x82f70bd8,34,3),('KPU_SECONDARY_WIDE_CURRENT',0x82f70c68,34,3),
    ('KAU_SECONDARY_LENGTHS_CF8',0x82f70cf8,73,2),('KAU_SECONDARY_LENGTHS_D90',0x82f70d90,64,2)]
mode_facts={}
for name,a,symbols,bits in modes:
    count=1+(1<<bits)*((symbols+1)//2)
    assert rd(a,1)==bytes([bits]) and a+count<=PACKED_END
    mode_facts[name]={'address':hex(a),'symbols':symbols,'bits':bits,'bytes':count}

def rewrite_source(s):
    lengths=[('KAU_CODE_LENGTHS_A0',0x82123db0),('KAU_CODE_LENGTHS_C0',0x82123dc0),
             ('KAU_CODE_LENGTHS_E10',0x82123e10),('KAU_CODE_LENGTHS_E38',0x82123e38)]
    data=literal('KAU_CODE_LENGTHS',CODE_START,code)+'\n\n'+literal('KAU_PACKED_SECONDARY_LENGTHS',PACKED_START,packed)
    offsets=['    const u8* const '+name+' = KAU_CODE_LENGTHS + '+f'0x{a-CODE_START:X};' for name,a in lengths]
    offsets+=['    const u8* const '+name+' = KAU_PACKED_SECONDARY_LENGTHS + '+f'0x{a-PACKED_START:X};'
              for name,a,_,_ in modes]
    s=replace(s,'#include "SDKs/EATech/rwmovie/CLocalHuffman.h"\n',
        '#include "SDKs/EATech/rwmovie/CLocalHuffman.h"\n\nnamespace\n{\n'+data+'\n\n'+'\n'.join(offsets)+'\n}\n')
    return s.rstrip()+"""

// ARTIST 0x82AC1B18. Select a packed code set and consume its index width.
void CLocalHuffman::setCodes(int liMode)
{
    const bool lbLegacyMode = liMode < 13;
    switch (muBits)
    {
    case 0:
    case 1:
        mpNextByte = lbLegacyMode ? KPU_SECONDARY_01_LEGACY : KPU_SECONDARY_01_CURRENT;
        break;
    case 2:
    case 3:
        mpNextByte = lbLegacyMode ? KPU_SECONDARY_23_LEGACY : KPU_SECONDARY_23_CURRENT;
        break;
    case 4:
        mpNextByte = lbLegacyMode ? KPU_SECONDARY_4_LEGACY : KPU_SECONDARY_4_CURRENT;
        break;
    case 8:
        mpaCodeLengths = KAU_CODE_LENGTHS_E38;
        return;
    case 10:
        return;
    default:
        if (static_cast<s32>(muBits) > 4)
            mpNextByte = lbLegacyMode ? KPU_SECONDARY_WIDE_LEGACY : KPU_SECONDARY_WIDE_CURRENT;
        break;
    }

    muSecondaryBits = *mpNextByte++;
    muSecondaryTableSize = 1u << muSecondaryBits;
}
"""
def rewrite_header(s):
    a=s.index('// The five code-length tables')
    b=s.index('// =====================================================================================',a)
    s=s[:a]+s[b:]
    a=s.index('// ---- static code-length / secondary tables')
    b=s.index('class CLocalHuffman',a)
    s=s[:a]+s[b:]
    a=s.index('    // CLocalHuffman::setCodes')
    b=s.index('\nprotected:',a)
    return s[:a]+"""    // Select the mode's packed code table for this index width.
    void setCodes(int liMode);
"""+s[b:]
def rewrite_alt(s):
    a=s.index('// Foreign codec global')
    b=s.index('// -------------------------------------------------------------------------------------',a)
    s=s[:a]+'// ARTIST dword_8322C3F0 biases the emitted short-code lengths.\nint g_iAltTablesShortCodeFlag = 0;\n\n'+s[b:]
    s=replace(s,"// flag, then for every table clear its code buffer and rebuild its code words. Returns the\n// last table's setCodes result.",
              "// flag, then for every table clear its code buffer and select its packed code set.")
    s=replace(s,'int CAltTablesEncoder::clear(int liMode, int liSymbolCount)','void CAltTablesEncoder::clear(int liMode, int liSymbolCount)')
    s=replace(s,'    int liResult = 0;\n    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)\n',
                 '    for (int liTable = 0; liTable < static_cast<int>(muNumTables); ++liTable)\n')
    s=replace(s,'        liResult = lpEnc->setCodes(liMode);\n    }\n    return liResult;','        lpEnc->setCodes(liMode);\n    }')
    return s
def rewrite_alt_header(s):
    a=s.index('    // Reset for a new frame at the given table mode')
    b=s.index('    // Measure the frame',a)
    return s[:a]+"""    // Reset for a new frame, clear every code buffer and select the packed tables.
    void clear(int liMode, int liSymbolCount);

"""+s[b:]
paths=(SOURCE,HEADER,ALT,ALT_HDR)
for path,fn in zip(paths,(rewrite_source,rewrite_header,rewrite_alt,rewrite_alt_header)):
    change(path,fn)
assert set(git('diff','--name-only').splitlines())==set(paths)
git('diff','--check','--',*paths)
args=json.loads((OUT.parent/'gui-queue-length-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
for p in (SOURCE,ALT,ENCODER):
    args+=['--source',p]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==3 and not report['failed']
rows={r['source']:r for r in report['results']}
assert all(r['machine']==498 for r in rows.values())
obj=pathlib.Path(rows[SOURCE]['object']).read_bytes()
assert obj.count(code)==obj.count(packed)==1
asm=pathlib.Path(rows[SOURCE]['assembly']).read_text(errors='replace')
assert '?setCodes@CLocalHuffman@@QAAXH@Z' in asm
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Restore native Huffman tables and mode selection\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in paths},
         'artist_code_table':{'start':hex(CODE_START),'end_exclusive':hex(CODE_END),'sha256':hashlib.sha256(code).hexdigest()},
         'artist_packed_tables':{'start':hex(PACKED_START),'end_exclusive':hex(PACKED_END),'sha256':hashlib.sha256(packed).hexdigest()},
         'packed_mode_entries':mode_facts,'exact_data_bytes_in_native_object':True,
         'artist_set_codes':'0x82AC1B18','artist_clear_sole_call':'0x82A99224, result unused',
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
