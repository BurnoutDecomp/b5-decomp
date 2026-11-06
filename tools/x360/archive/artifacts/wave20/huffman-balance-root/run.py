import hashlib,json,os,pathlib,subprocess,sys

OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='f0121fdb8187d839b1e69ef6bc46cad476c69a9c'
PFX='src/SDKs/EATech/rwmovie/'
SOURCE=PFX+'CLocalHuffman.cpp'
CALLER=PFX+'CLocalHuffmanEncoder.cpp'

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def update(p,old,new):
    b=p.read_bytes()
    eol='\r\n' if b'\r\n' in b else '\n'
    s=b.decode().replace('\r\n','\n')
    assert s.count(old)==1
    p.write_bytes(s.replace(old,new).replace('\n',eol).encode())

assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
anchor='// CLocalHuffman::CLocalHuffman @0x82AC1920 -- build a canonical (length-limited)'
body="""// ARTIST 0x82AC17D0. Assign balanced prefix codes in the incoming symbol order.
u32* generateHuffman_balanced(u32* lpauLengths, int liLengthStride, u32 luNumSymbols,
                              u32* lpauCodesOut, int liCodeStride)
{
    const s32 KI_CODE_LIMIT = 20;
    const u32 KU_EXHAUSTED = 1u << KI_CODE_LIMIT;
    u32 lauNextCode[KI_CODE_LIMIT] = { KU_EXHAUSTED };
    s32 liExhaustedLength = 0;

    for (s32 liSymbol = 0; liSymbol < static_cast<s32>(luNumSymbols); ++liSymbol)
    {
        const s32 liLength = static_cast<s32>(*lpauLengths);
        lpauLengths += liLengthStride;
        if (liLength >= KI_CODE_LIMIT)
            break;

        const u32 luPrevious = lauNextCode[liLength];
        u32 luStep = 1u << (KI_CODE_LIMIT - liLength);
        *lpauCodesOut = luPrevious >> (KI_CODE_LIMIT - liLength);
        lpauCodesOut += liCodeStride;

        const u32 luNext = (luPrevious & luStep) != 0
                         ? lauNextCode[liLength - 1] : luPrevious + luStep;
        lauNextCode[liLength] = luNext;
        if (luNext == KU_EXHAUSTED)
            liExhaustedLength = liLength;

        for (s32 liShorter = liLength - 1; liShorter > liExhaustedLength; --liShorter)
        {
            if (lauNextCode[liShorter] != luPrevious)
                break;
            luStep <<= 1;
            const u32 luShorterNext = (luPrevious & luStep) != 0
                                    ? lauNextCode[liShorter - 1] : luPrevious + luStep;
            lauNextCode[liShorter] = luShorterNext;
            if (luShorterNext == KU_EXHAUSTED)
                liExhaustedLength = liShorter;
        }

        for (s32 liLonger = liLength + 1; liLonger < KI_CODE_LIMIT; ++liLonger)
        {
            if (lauNextCode[liLonger] != luPrevious)
                break;
            lauNextCode[liLonger] = luNext;
        }
    }
    return lpauLengths;
}

"""
update(WORK/SOURCE,anchor,body+anchor)
update(WORK/CALLER,'// A separate, not-yet-reconstructed X360 TU (generateHuffman_balanced @todo).',
       '// Defined with the native decoder tables at ARTIST 0x82AC17D0.')
paths=(SOURCE,CALLER)
git('diff','--check','--',*paths)
assert set(git('diff','--name-only').splitlines())==set(paths)
args=json.loads((OUT.parent/'huffman-mv-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
for p in paths:args+=['--source',p]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==2 and not report['failed']
rows={r['source']:r for r in report['results']}
assert all(r['machine']==498 for r in rows.values())
symbol='?generateHuffman_balanced@@YAPAIPAIHI0H@Z'
a=pathlib.Path(rows[SOURCE]['assembly']).read_text(errors='replace')
b=pathlib.Path(rows[CALLER]['assembly']).read_text(errors='replace')
assert 'Begin code for function: '+symbol in a
assert any(symbol in line and ' bl ' in line for line in b.splitlines())
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Restore ARTIST balanced Huffman code generation\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',*paths)
assert set(git('diff','--cached','--name-only').splitlines())==set(paths)
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'paths':{p:sha(WORK/p) for p in paths},
         'artist_body':'0x82AC17D0..0x82AC191F','artist_caller':'0x82A7E014','symbol':symbol,
         'facts':['20 signed-checked length slots with slot zero initialized to 1 << 20.',
                  'Code cursor advances by input/output strides; rejected lengths still consume the input word.',
                  'Carries propagate to shorter equal-prefix lengths and replace longer equal-prefix slots.'],
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
