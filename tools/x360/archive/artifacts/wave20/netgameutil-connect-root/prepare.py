from pathlib import Path
import json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
selector='eef83979f59de4715680756e648f2d2fec0d4a7a'
advt='204c16ab9b817e35efb37858adbd6f75e91a385e'
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')==selector and not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r29-netgameutil-connect',advt)
git('cherry-pick','ebb096a8b842bd5f49bffa51235d3d5d0f7c4f41',selector)
(d/'parent.txt').write_text(git('rev-parse','HEAD')+'\n')
src='vendor/dirtysdk/src/x360/netgameutil_x360.cpp'
hdr='vendor/dirtysdk/include/netgameutil.h'
p=w/hdr
b=p.read_bytes();eol=b'\r\n' if b'\r\n' in b else b'\n'
old=b'    s32 NetGameUtilStatus(NetGameUtilRefT* ref, s32 selector, void* pBuffer, s32 size);'
assert b.count(old)==1
p.write_bytes(b.replace(old,old+eol+
    b'    s32 NetGameUtilConnect(NetGameUtilRefT* ref, s32 connection, const char* address);'+eol+
    b'    void* NetGameUtilComplete(NetGameUtilRefT* ref);'))
p=w/src
b=p.read_bytes();eol=b'\r\n' if b'\r\n' in b else b'\n'
old=b'#include "dirtymem.h"'
assert b.count(old)==1
b=b.replace(old,old+eol+b'#include "platform.h"'+eol+b'#include "protoadvt.h"')
old=b'struct ProtoAdvtRef;'+eol+b'extern "C" void ProtoAdvtDestroy(ProtoAdvtRef* ref);'
assert b.count(old)==1
decl=r'''struct CommUDPRef;
struct CommSRPRef;
struct CommTCPRef;
extern "C" CommUDPRef* CommUDPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
extern "C" CommSRPRef* CommSRPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);
extern "C" CommTCPRef* CommTCPConstruct(s32 maxWidth, s32 maxInput, s32 maxOutput);'''
b=b.replace(old,decl.encode().replace(b'\n',eol)).replace(b'ProtoAdvtRef*',b'ProtoAdvtRefT*')
body=r'''
// ARTIST 0x82B2F720
extern "C" s32 NetGameUtilConnect(NetGameUtilRefT* ref, s32 connection, const char* address)
{
    const s32 mode = connection & 3;
    if (mode == 0 || (connection & 0x700) == 0)
    {
        NetPrintfCode("netgameutil: invalid conn param\n");
        return -1;
    }

    std::strcpy(ref->addr, address);
    ref->hosting = (connection >> 1) & 1;
    ref->proto = connection & ~3u;
    NetPrintfCode("netgameutil: connect %d %s\n", connection, address);

    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (port.comm)
            port.comm->Destroy(port.comm);
        port.comm = 0;
        port.tick = 0;
    }

    if (mode == 3)
    {
        if (!ref->find)
        {
            DirtyMemGroupEnter(ref->memgroup);
            ProtoAdvtRefT* find = ProtoAdvtConstruct(8);
            DirtyMemGroupLeave();
            ref->find = find;
        }
        // ARTIST's 0x8324EBD8 note is empty and only read by this call.
        ProtoAdvtAnnounce(ref->find, "GmUtil", address, "", "TCP:~1:1024\tUDP:~1:1024", 0);
        return 0;
    }

    DirtyMemGroupEnter(ref->memgroup);
    if (connection & 0x100)
    {
        ref->port[0].comm = reinterpret_cast<CommRef*>(
            CommUDPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[0].wait = 0;
    }
    else if (connection & 0x400)
    {
        ref->port[1].comm = reinterpret_cast<CommRef*>(
            CommSRPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[1].wait = 0;
    }
    else if (connection & 0x200)
    {
        ref->port[2].comm = reinterpret_cast<CommRef*>(
            CommTCPConstruct(ref->maxwid, ref->maxinp, ref->maxout));
        ref->port[2].wait = (ref->port[0].comm || ref->port[1].comm) ? 5000 : 0;
    }

    s32 result = 0;
    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (!port.comm)
            continue;
        if (port.comm->Control)
        {
            port.comm->Control(port.comm, 0x636c6964, ref->clientid, 0); // clid
            port.comm->Control(port.comm, 0x72636964, ref->rclientid, 0); // rcid
        }
        if (mode & 2)
            result = port.comm->Connect(port.comm, address);
        else if (mode & 1)
            result = port.comm->Listen(port.comm, address);
        ref->pSocket = port.comm->sockptr;
        break;
    }
    DirtyMemGroupLeave();
    return result;
}

// ARTIST 0x82B2F9A0
extern "C" void* NetGameUtilComplete(NetGameUtilRefT* ref)
{
    if (ref->find && !ref->port[0].comm && !ref->port[1].comm &&
        !ref->port[2].comm && !ref->port[3].comm)
    {
        u32 localAddress;
        const u32 remoteAddress = ProtoAdvtLocate(ref->find, "GmUtil", ref->addr, &localAddress, 0);
        if (remoteAddress != 0)
        {
            NetPrintfCode("netgameutil: located peer=%08x, host=%08x\n", remoteAddress, localAddress);
            char connectAddress[256];
            s32 connection;
            if (remoteAddress > localAddress)
            {
                ref->peerip = localAddress;
                ref->hostip = remoteAddress;
                ds_snzprintf(connectAddress, sizeof(connectAddress), "%d.%d.%d.%d%s",
                             remoteAddress >> 24, (remoteAddress >> 16) & 0xff,
                             (remoteAddress >> 8) & 0xff, remoteAddress & 0xff, ref->addr);
                connection = ref->proto | 2;
            }
            else
            {
                ref->hostip = localAddress;
                ref->peerip = remoteAddress;
                ds_strnzcpy(connectAddress, ref->addr, sizeof(connectAddress));
                connection = ref->proto | 1;
            }
            NetGameUtilConnect(ref, connection, connectAddress);
        }
    }

    u32 now = 0;
    s32 winner = 0;
    for (; winner < 4; ++winner)
    {
        NetGameUtilRefT::Port& port = ref->port[winner];
        if (!port.comm)
            continue;
        if (now == 0)
            now = port.comm->Tick(port.comm);
        if (port.comm->Status(port.comm) != 3)
            continue;
        if (port.tick == 0)
            port.tick = now;
        if (now >= port.tick + port.wait)
            break;
    }
    if (winner == 4)
        return 0;

    CommRef* comm = ref->port[winner].comm;
    ref->hostip = comm->hostip;
    ref->hostport = comm->hostport;
    ref->peerip = comm->peerip;
    ref->peerport = comm->peerport;
    NetPrintfCode("netgameutil: connection complete\n");

    for (s32 i = 0; i < 4; ++i)
    {
        NetGameUtilRefT::Port& port = ref->port[i];
        if (i != winner && port.comm)
        {
            port.comm->Destroy(port.comm);
            port.comm = 0;
        }
    }
    if (ref->advt)
    {
        ProtoAdvtDestroy(ref->advt);
        ref->advt = 0;
    }
    if (ref->find)
    {
        ProtoAdvtDestroy(ref->find);
        ref->find = 0;
    }
    return ref->port[winner].comm;
}
'''
p.write_bytes(b+body.encode().replace(b'\n',eol))
git('diff','--check')
prior=json.loads((d.parent/'netgameutil-lifetime-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
for path in (src,'vendor/dirtysdk/src/x360/protoadvt_x360.cpp','vendor/dirtysdk/src/x360/connapi_x360.cpp'):
    args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
