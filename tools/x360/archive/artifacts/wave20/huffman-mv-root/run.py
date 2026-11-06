import hashlib,json,os,pathlib,subprocess,sys

OUT=pathlib.Path(__file__).resolve().parent
WORK=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
PARENT='4b49be708ec1e0718f5c32c42785e47a89ec3176'
SOURCE='src/SDKs/EATech/rwmovie/CLocalHuffmanEncoder.cpp'
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
from x360rd import u32

def git(*args):
    return subprocess.check_output(['git',*args],cwd=WORK).decode().strip()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert git('rev-parse','HEAD')==PARENT and not git('status','--porcelain=v1')
assert u32(0x82815570)==0x7c8a2378 and u32(0x82F264A8)==4
raw={'KAU_MV_CODE_BITS':(0x82f413bc,6),'KAU_RTC_EXTRA_BITS':(0x82f413d4,4),'KAU_RTC_PREFIX':(0x82f413e4,4)}
tables={name:[u32(a+i*4) for i in range(n)] for name,(a,n) in raw.items()}
decl='\n'.join(f'    const u32 {n}[] = {{ '+', '.join(map(str,vs))+' };' for n,vs in tables.items())
bodies="""
    // ARTIST 0x82A7E1A0
    void EncodeMvComponent(int liComponent, int* lpiIndexOut, int* lpiCodeOut, int* lpiShiftOut)
    {
        const u32 luComponent = static_cast<u32>(liComponent);
        const u32 luSign = (luComponent >> 16) & 1u;
        const u32 luMagnitude = liComponent < 0 ? 0u - luComponent : luComponent;
        const s32 liMagnitude = static_cast<s32>(luMagnitude);

        u32 luCode = 0;
        s32 liIndex = 0;
        if (liMagnitude != 0)
        {
            if (liMagnitude < 3)
            {
                liIndex = 1;
                luCode = (luMagnitude << 1) - 2u;
            }
            else if (liMagnitude < 7)
            {
                liIndex = 2;
                luCode = (luMagnitude - 3u) << 1;
            }
            else if (liMagnitude < 15)
            {
                liIndex = 3;
                luCode = (luMagnitude - 7u) << 1;
            }
            else if (liMagnitude < 31)
            {
                liIndex = 4;
                luCode = (luMagnitude - 15u) << 1;
            }
            else
            {
                liIndex = 5;
                luCode = (luMagnitude - 31u) << 1;
            }
            luCode |= luSign;
        }

        *lpiCodeOut = static_cast<s32>(luCode);
        *lpiIndexOut = liIndex;
        *lpiShiftOut = KAU_MV_CODE_BITS[liIndex];
    }

    // ARTIST 0x82A7E258
    void EncodeRtcmvComponent(int liComponent, int* lpiIndexOut, int* lpiCodeOut,
                              int* lpiShiftOut, int liBits)
    {
        const u32 luComponent = static_cast<u32>(liComponent);
        const u32 luSign = (luComponent >> 16) & 1u;
        const u32 luMagnitude = liComponent < 0 ? 0u - luComponent : luComponent;
        const s32 liMagnitude = static_cast<s32>(luMagnitude);

        if (liMagnitude == 0)
        {
            *lpiCodeOut = 0;
            *lpiIndexOut = 0;
            *lpiShiftOut = 0;
            return;
        }
        if (liMagnitude < 3)
        {
            *lpiCodeOut = static_cast<s32>(((luMagnitude << 1) - 2u) | luSign);
            *lpiIndexOut = 1;
            *lpiShiftOut = 2;
            return;
        }

        s32 liIndex;
        u32 luGroup;
        u32 luShift;
        u32 luSuffix;
        if (liMagnitude < 7)
        {
            liIndex = 2;
            luGroup = luMagnitude - 3u;
            luShift = 1;
            luSuffix = 0;
        }
        else if (liMagnitude < 15)
        {
            liIndex = 3;
            luGroup = (luMagnitude - 7u) >> 1;
            luShift = 2;
            luSuffix = (~luMagnitude & 1u) << 1;
        }
        else if (liMagnitude < 31)
        {
            liIndex = 4;
            luGroup = (luMagnitude - 15u) >> 2;
            luShift = 3;
            luSuffix = ((luMagnitude + 1u) << 1) & 0x06u;
        }
        else
        {
            liIndex = 5;
            luShift = liBits != 0 ? 5u : 6u;
            luGroup = (luMagnitude - 31u) >> (luShift - 1u);
            luSuffix = ((luMagnitude + 1u) << 1) & ((1u << luShift) - 2u);
        }

        *lpiIndexOut = liIndex;
        *lpiCodeOut = static_cast<s32>((KAU_RTC_PREFIX[luGroup] << luShift) | luSuffix | luSign);
        *lpiShiftOut = luShift + KAU_RTC_EXTRA_BITS[luGroup];
    }
"""
p=WORK/SOURCE
b=p.read_bytes()
eol='\r\n' if b'\r\n' in b else '\n'
s=b.decode().replace('\r\n','\n')
a=s.index('// -------------------------------------------------------------------------------------\n// Per-motion-vector-component')
z=s.index('CGS_STATIC_ASSERT',a)
s=s[:a]+'namespace\n{\n'+decl+'\n'+bodies+'}\n\n'+s[z:]
p.write_bytes(s.replace('\n',eol).encode())
git('diff','--check','--',SOURCE)
assert git('diff','--name-only')==SOURCE
args=json.loads((OUT.parent/'huffman-encoder-ctor-root/command.json').read_text())
args=args[:args.index('--source')]
args[args.index('--out')+1]=str(OUT/'native1')
args+=['--source',SOURCE]
temp=OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report_path=OUT/'native1/compile-results.json'
report=json.loads(report_path.read_text())
assert report['requested']==report['passed']==1 and not report['failed']
row=report['results'][0]
assert row['machine']==498
asm=pathlib.Path(row['assembly']).read_text(errors='replace')
assert '?EncodeMvComponent@@YAHHPAH00@Z' not in asm
assert '?EncodeRtcmvComponent@@YAHHPAH00H@Z' not in asm
assert 'EncodeMvComponent' in asm and 'EncodeRtcmvComponent' in asm
msg=OUT/'COMMIT_MESSAGE.txt'
msg.write_text('Restore native movie motion-vector coding helpers\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',SOURCE)
assert git('diff','--cached','--name-only')==SOURCE
git('commit','--file',str(msg))
assert not git('status','--porcelain=v1')
receipt={'parent':PARENT,'commit':git('rev-parse','HEAD'),'path':SOURCE,'source_sha256':sha(p),
         'artist':{'EncodeMvComponent':'0x82A7E1A0..0x82A7E253','EncodeRtcmvComponent':'0x82A7E258..0x82A7E40B'},
         'tables':{name:{'address':hex(raw[name][0]),'words':words} for name,words in tables.items()},
         'native_calls_consume_only_output_parameters':['0x82A7E490','0x82A7E4A4','0x82A7E604','0x82A7E61C'],
         'report':str(report_path),'report_sha256':sha(report_path)}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
