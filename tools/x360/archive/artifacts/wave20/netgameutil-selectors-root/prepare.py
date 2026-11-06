from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='ebb096a8b842bd5f49bffa51235d3d5d0f7c4f41'
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')=='da8020c3ad575c2bc488192028945fbc6435bde8' and not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r29-netgameutil-selectors',base)
header='vendor/dirtysdk/include/netgameutil.h'
src='vendor/dirtysdk/src/x360/netgameutil_x360.cpp'
p=w/header
b=p.read_bytes();eol=b'\r\n' if b'\r\n' in b else b'\n'
old=b'    void NetGameUtilDestroy(NetGameUtilRefT* ref);'
assert b.count(old)==1
p.write_bytes(b.replace(old,old+eol+
    b'    void NetGameUtilControl(NetGameUtilRefT* ref, s32 selector, s32 value);'+eol+
    b'    s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size);'))
p=w/src
b=p.read_bytes();eol=b'\r\n' if b'\r\n' in b else b'\n'
body=r'''
// ARTIST 0x82B2F620
extern "C" void NetGameUtilControl(NetGameUtilRefT* ref, s32 selector, s32 value)
{
    switch (selector)
    {
    case 0x636c6964: // clid
        ref->clientid = value;
        NetPrintfCode("netgameutil: setting clid=0x%08x\n", static_cast<u32>(value));
        break;
    case 0x72636964: // rcid
        ref->rclientid = value;
        NetPrintfCode("netgameutil: setting rcid=0x%08x\n", static_cast<u32>(value));
        break;
    case 0x6d776964: // mwid
        if (value <= 1200)
        {
            ref->maxwid = value + 12;
            NetPrintfCode("netgameutil: setting mwid=%d\n", ref->maxwid);
        }
        else
        {
            NetPrintfCode("netgameutil: mwid value of %d is too large\n", value);
        }
        break;
    case 0x6d696e70: // minp
        ref->maxinp = value;
        NetPrintfCode("netgameutil: setting minp=%d\n", value);
        break;
    case 0x6d6f7574: // mout
        ref->maxout = value;
        NetPrintfCode("netgameutil: setting mout=%d\n", value);
        break;
    case 0x61647666: // advf
        ref->advtfreq = value;
        NetPrintfCode("netgameutil: setting advf=%d\n", value);
        break;
    }
}

// ARTIST 0x82B2FBD8
extern "C" s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size)
{
    switch (selector)
    {
    case 0x686f7374: return ref->hosting == 0; // host
    case 0x6a6f696e: return ref->hosting == 1; // join
    case 0x686f6970: return static_cast<s32>(ref->hostip);   // hoip
    case 0x68707274: return static_cast<s32>(ref->hostport); // hprt
    case 0x70656970: return static_cast<s32>(ref->peerip);   // peip
    case 0x70707274: return static_cast<s32>(ref->peerport); // pprt
    case 0x736f636b: // sock
        if (size == sizeof(ref->pSocket))
        {
            std::memcpy(pBuffer, &ref->pSocket, sizeof(ref->pSocket));
            return sizeof(ref->pSocket);
        }
        break;
    }
    return -1;
}
'''
p.write_bytes(b+body.encode().replace(b'\n',eol))
git('diff','--check')
prior=json.loads((d.parent/'netgameutil-lifetime-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args+=prior[prior.index('--source'):]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
