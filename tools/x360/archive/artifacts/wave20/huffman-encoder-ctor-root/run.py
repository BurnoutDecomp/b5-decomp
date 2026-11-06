import hashlib,json,os,pathlib,subprocess,sys

OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='1c17b8e18344c474086e3ccdd59d6a79b424e63c'
PREFIX='src/SDKs/EATech/rwmovie/'
SOURCE=PREFIX+'CLocalHuffmanEncoder.cpp'
HEADER=PREFIX+'CLocalHuffmanEncoder.h'
ALT=PREFIX+'CAltTablesEncoder.cpp'
ALT_HDR=PREFIX+'CAltTablesEncoder.h'

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def update(path,fn):
    p=WORK/path
    b=p.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    p.write_bytes(fn(b.decode().replace('\r\n','\n')).replace('\n',eol).encode())
def replace(s,old,new):
    assert s.count(old)==1,old
    return s.replace(old,new)
assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')

def header(s):
    a=s.index('// Object referenced through CLocalHuffmanEncoder::mpState')
    b=s.index('class CLocalHuffmanEncoder :',a)
    s=s[:a]+s[b:]
    s=replace(s,'    // (table index, owning CAltTablesEncoder, error out). Body is a separate X360 TU.',
              '    // ARTIST 0x82A7DDD0. Allocate the encoder-side code and count arrays.')
    s=replace(s,'    SLocalHuffmanEncoderState* mpState;         // +0x48  owning CAltTablesEncoder (via the stand-in above)',
              '    CAltTablesEncoder*          mpState;         // +0x48  owning CAltTablesEncoder')
    return s
def alt_header(s):
    s=replace(s,'private:\n    static const int KI_NUM_TABLES',
              'private:\n    friend class CLocalHuffmanEncoder;\n\n    static const int KI_NUM_TABLES')
    return replace(s,'    u32                   muPad24;            // +0x24  (unobserved)',
                     '    u32                   muMvSearchBits;     // +0x24  motion-vector search range bits')
def alt_source(s):
    return replace(s,'lpEnc->mpState->muDirty != 0','lpEnc->mpState->muFrameFlag != 0')
def source(s):
    s=replace(s,"// through CLocalHuffmanEncoder::mpState (the owning CAltTablesEncoder, modelled by the opaque\n// SLocalHuffmanEncoderState in the header to keep the include cycle open).",
                '// through CLocalHuffmanEncoder::mpState, the owning CAltTablesEncoder.')
    s=replace(s,'#include "SDKs/EATech/rwmovie/COutBitStream.h"',
                '#include "SDKs/EATech/rwmovie/CAltTablesEncoder.h"\n#include "SDKs/EATech/rwmovie/COutBitStream.h"')
    s=s.replace('mpState->muDirty','mpState->muFrameFlag').replace('mpState->mpuSymbolRecord','mpState->mpSymbolCursor')
    pos=s.index('// -------------------------------------------------------------------------------------\n// CLocalHuffmanEncoder::encodeHeader')
    return s[:pos]+"""CGS_STATIC_ASSERT((sizeof(CLocalHuffmanEncoder) == 0x4C), "ARTIST encoder layout");

// ARTIST 0x82A7DDD0
CLocalHuffmanEncoder::CLocalHuffmanEncoder(int liTableIndex, CAltTablesEncoder* lpParent, u32* lpuError)
    : CLocalHuffman(static_cast<u32>(liTableIndex), lpuError)
{
    mpState = lpParent;
    muCurrentCode = 0;
    mpauCodeBuffer = 0;
    mpauCodeData = 0;
    muEmittedCode = 0xFFFFFFFFu;

    u32 luLengthBytes = muNumSymbols * sizeof(u32);
    if (muNumSymbols > 0x3FFFFFFFu)
        luLengthBytes = 0xFFFFFFFFu;
    mpauCodeData = static_cast<u32*>(XMemAlloc(luLengthBytes, KU_HUFFMAN_XMEM_ATTRIBUTES));

    const u32 luCount = 2u * muNumSymbols;
    u32 luBufferBytes = luCount * sizeof(u32);
    if (luCount > 0x3FFFFFFFu)
        luBufferBytes = 0xFFFFFFFFu;
    mpauCodeBuffer = static_cast<u32*>(XMemAlloc(luBufferBytes, KU_HUFFMAN_XMEM_ATTRIBUTES));

    if (mpauCodeData != 0 && mpauCodeBuffer != 0)
    {
        muNeedHeader = 0;
        muHistoryCode = 0;
        muDirtyFlag = 0;
        mpauCodes = mpauCodeBuffer + muNumSymbols;
        return;
    }

    if (mpauCodeData != 0)
    {
        XMemFree(mpauCodeData, KU_HUFFMAN_XMEM_ATTRIBUTES);
        mpauCodeData = 0;
    }
    if (mpauCodeBuffer != 0)
    {
        XMemFree(mpauCodeBuffer, KU_HUFFMAN_XMEM_ATTRIBUTES);
        mpauCodeBuffer = 0;
    }
    *lpuError = 1;
}

"""+s[pos:]
paths=(SOURCE,HEADER,ALT,ALT_HDR)
for p,fn in zip(paths,(source,header,alt_source,alt_header)):
    update(p,fn)
git('diff','--check','--',*paths)
assert set(git('diff','--name-only').splitlines())==set(paths)
args=json.loads((OUT.parent/'huffman-tables-root/command.json').read_text())
args[args.index('--out')+1]=str(OUT/'native1')
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==3 and not report['failed']
rows={r['source']:r for r in report['results']}
assert all(r['machine']==498 for r in rows.values())
asm=pathlib.Path(rows[SOURCE]['assembly']).read_text(errors='replace')
symbol='??0CLocalHuffmanEncoder@@QAA@HPAVCAltTablesEncoder@@PAI@Z'
body=asm.split('Begin code for function: '+symbol,1)[1].split('End code for function:',1)[0]
assert sum(' bl ' in l and 'XMemAlloc' in l for l in body.splitlines())==2
assert sum(' bl ' in l and 'XMemFree' in l for l in body.splitlines())==2
assert 'CLocalHuffman@@QAA' in body
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Recover native Huffman encoder allocation and ownership\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in paths},
         'artist_constructor':'0x82A7DDD0..0x82A7DEE3','artist_caller':'0x82A7EDB8',
         'real_parent_member_offsets':{'muFrameFlag':8,'muMvSearchBits':36,'mpSymbolCursor':88},
         'facts':['0x4C-byte native derived object','Two XMemAlloc calls and both failure cleanup paths',
                  'Code words alias second half of the count-buffer allocation.'],
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
