import hashlib, json, os, pathlib, subprocess, sys

BASE = pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts')
OUT = pathlib.Path(__file__).resolve().parent
WORK = pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
SOURCE = 'src/SDKs/Xam/CBaseEndianBuffer.cpp'
PARENT = 'd49dc118b8fd5a28206025f7f8a1ac92feb95d52'
BODY = """
// CBaseEndianBuffer::GetDataAndAdvance @ 0x8297FEB0
s32 CBaseEndianBuffer::GetDataAndAdvance(void* lpOut, u32* lpSize, u32 lbSwapEndian)
{
    void* lpSrc;
    const s32 lResult = GetPointerAndAdvance(&lpSrc, lpSize);
    if (lResult < 0)
    {
        return lResult;
    }

    if (!lbSwapEndian)
    {
        memcpy(lpOut, lpSrc, *lpSize);
        return lResult;
    }

    switch (*lpSize)
    {
    case 1:
        *static_cast<u8*>(lpOut) = *static_cast<const u8*>(lpSrc);
        break;
    case 2:
        *static_cast<u16*>(lpOut) = _byteswap_ushort(*static_cast<const u16*>(lpSrc));
        break;
    case 4:
        *static_cast<u32*>(lpOut) = _byteswap_ulong(*static_cast<const u32*>(lpSrc));
        break;
    case 8:
        {
            const u32* lpWords = static_cast<const u32*>(lpSrc);
            const u64 luLow = _byteswap_ulong(lpWords[0]);
            const u64 luHigh = _byteswap_ulong(lpWords[1]);
            *static_cast<u64*>(lpOut) = (luHigh << 32) | luLow;
            break;
        }
    default:
        return static_cast<s32>(0x8007000D);
    }

    return lResult;
}
"""

def git(*args):
    return subprocess.check_output(['git', *args], cwd=WORK).decode().strip()

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

assert git('rev-parse','HEAD') == PARENT and not git('status','--porcelain=v1')
assert git('branch','--show-current') == 'codex/experimental-x360-r23-thread-layout'
src = WORK / SOURCE
original = src.read_bytes()
line = b'\r\n' if b'\r\n' in original else b'\n'
def encoded(s): return s.encode().replace(b'\n', line)
before = encoded('#include <cstring> // memcpy\n')
after = before + encoded('#include <cstdlib> // _byteswap_ushort, _byteswap_ulong\n')
assert original.count(before) == 1
marker = encoded('// ---------------------------------------------------------------------------\n// CBaseEndianBuffer::WriteBytes @ 0x8297FF90')
assert original.count(marker) == 1
src.write_bytes(original.replace(before, after).replace(marker, encoded(BODY.strip()+'\n\n')+marker))
subprocess.run(['git','diff','--check','--',SOURCE],cwd=WORK,check=True)
reference = json.loads((BASE/'wave17/network-platform-terra/connapi-control-receipt.json').read_text())
previous = reference['invocation']
args = [sys.executable,'-X','utf8','-B','tools/build_x360.py','--sdk', reference['sdk'],
        '--out', str(OUT/'native1'),'--jobs','1','--source',SOURCE]
args.extend(x for x in previous if x.startswith('--compiler-arg='))
assert len([x for x in args if x.startswith('--compiler-arg=/I')]) == 9
temp = OUT/'temp'
temp.mkdir()
(OUT/'command.json').write_text(json.dumps(args,indent=2)+'\n')
subprocess.run(args,cwd=WORK,env=dict(os.environ,TEMP=str(temp),TMP=str(temp)),check=True)
report = json.loads((OUT/'native1/compile-results.json').read_text())
assert report['requested'] == report['passed'] == 1 and not report['failed']
row = report['results'][0]
assert row['machine'] == 498
assembly = pathlib.Path(row['assembly'])
listing = assembly.read_text(errors='replace')
assert '?GetDataAndAdvance@CBaseEndianBuffer@@QAAHPAXPAII@Z' in listing
assert '_byteswap_' not in '\n'.join(x for x in listing.splitlines() if 'bl ' in x or '\tbl\t' in x), 'unexpected external call'
assert git('diff','--name-only') == SOURCE
message = OUT/'COMMIT_MESSAGE.txt'
message.write_text('Recover native Xam endian buffer reads\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('add','--',SOURCE)
assert git('diff','--cached','--name-only') == SOURCE
git('commit','--file',str(message))
assert not git('status','--porcelain=v1')
receipt = {'parent':PARENT,'commit':git('rev-parse','HEAD'),'source':SOURCE,'source_sha256':sha(src),
           'artist_body':'0x8297FEB0..0x8297FF8B','artist_direct_caller':'0x82980264','artist_get_pointer':'0x8297FE60',
           'facts':['Negative GetPointerAndAdvance result returned before any output read/write.',
                    'Size is reread after cursor advances; no swap copies the full size.',
                    'Swap cases use widths 1, 2, 4, 8. Width 8 reverses and exchanges the two 32-bit halves.',
                    'Other swap sizes return 0x8007000D after cursor advance.'],
           'report':str(OUT/'native1/compile-results.json'),'report_sha256':sha(OUT/'native1/compile-results.json'),
           'assembly_sha256':sha(assembly),'object_sha256':sha(pathlib.Path(row['object']))}
(OUT/'DELIVERY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['commit'])
