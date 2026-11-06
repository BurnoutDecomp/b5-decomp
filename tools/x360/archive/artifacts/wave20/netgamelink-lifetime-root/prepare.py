from pathlib import Path
import os,json,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
r26='3792b7d0f752405cbf160c530832492df09da639'
def git(*args):return subprocess.check_output(['git',*args],cwd=w).decode().rstrip()
assert not git('status','--porcelain=v1')
git('switch','-c','codex/experimental-x360-r28-netgamelink-lifetime',r26)
def edit(rel,changes):
    p=w/rel
    data=p.read_bytes()
    eol=b'\r\n' if b'\r\n' in data else b'\n'
    for before,after in changes:
        a=before.encode().replace(b'\n',eol)
        b=after.encode().replace(b'\n',eol)
        assert data.count(a)==1,(p,before,data.count(a))
        data=data.replace(a,b)
    p.write_bytes(data)

header='vendor/dirtysdk/include/netgamelink.h'
internal='vendor/dirtysdk/src/x360/dirtylib_x360_internal.h'
source='vendor/dirtysdk/src/x360/netgamelink_x360.cpp'
edit(header,[
('''// Send packets. With iLen == 1, pPacket is exactly one packet''',
 '''NetGameLinkRefT* NetGameLinkCreate(void* pCommRef, s32 owner, s32 buflen);
void NetGameLinkDestroy(NetGameLinkRefT* pRef);

// Send packets. With iLen == 1, pPacket is exactly one packet'''),
('''// Bodies (PC backend): ../src/pc/netgamelinkpc.cpp.''',
 '''// Native bodies are in ../src/x360/netgamelink_x360.cpp.'''),
])
edit(internal,[
('void NetCritInit(NetCritT*, const char*);','void NetCritInit(NetCritT*, const char*);\nvoid NetCritKill(NetCritT*);')
])
edit(source,[
('''#include "commall_x360_internal.h"
#include <cstddef>''','''#include "commall_x360_internal.h"
#include "dirtylib.h"
#include "dirtymem.h"
#include <cstddef>
#include <cstring>'''),
('''#include "dirtylib.h"
#include <cstring>
extern "C" u32 SocketHtonl''','''extern "C" u32 SocketHtonl'''),
('''CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, m_recvprocct) == 0x1f0), "link receive processing count");''',
 '''CGS_STATIC_ASSERT((offsetof(NetGameLinkRefT, m_recvprocct) == 0x1f0), "link receive processing count");
CGS_STATIC_ASSERT((sizeof(NetGameLinkRefT) == 0x1f8), "ARTIST link allocation");'''),
('''}
}

// ARTIST 0x82B1DC90.''','''}

// ARTIST 0x82B1D980
void NetGameLinkNotify(CommRef* port, s32)
{
    NetGameLinkRefT* ref = static_cast<NetGameLinkRefT*>(port->refptr);
    if (NetCritTry(&ref->crit))
    {
        Process(ref, NetTick());
        if (ref->m_buflen > 0 && ref->m_callproc && ref->m_callpend > 0)
        {
            ref->m_callpend = 0;
            ref->m_callproc(ref->m_calldata, 1);
        }
        NetCritLeave(&ref->crit);
    }
    else
    {
        ++ref->m_process;
    }
}

const s32 NETGAMELINK_MEMID = 0x6e676c6b;
}

// ARTIST 0x82B1DA28
extern "C" NetGameLinkRefT* NetGameLinkCreate(void* pCommRef, s32 owner, s32 buflen)
{
    const s32 group = DirtyMemGroupQuery();
    NetGameLinkRefT* ref = static_cast<NetGameLinkRefT*>(
        DirtyMemAlloc(sizeof(NetGameLinkRefT), NETGAMELINK_MEMID, group));
    if (!ref)
    {
        NetPrintfCode("netgamelink: unable to allocate module state\\n");
        return 0;
    }

    std::memset(ref, 0, sizeof(*ref));
    CommRef* port = static_cast<CommRef*>(pCommRef);
    ref->m_memgroup = group;
    ref->m_port = port;
    ref->m_portowner = owner;
    if (buflen < 4096)
        buflen = 4096;
    ref->m_buffer = static_cast<char*>(DirtyMemAlloc(buflen, NETGAMELINK_MEMID, group));
    ref->m_bufmax = buflen;
    ref->m_rttavg = 0;
    ref->m_rttdev = 0;

    const u32 tick = NetTick();
    ref->m_stats.isconn = 0;
    ref->m_stats.isopen = 0;
    ref->m_stats.conn = tick;
    ref->m_stats.senthide = tick;
    ref->m_stats.rcvdhide = tick;
    ref->m_stats.rcvdlast = tick;
    ref->m_stats.sentlast = tick;
    ref->m_stats.stattick = tick;
    for (s32 i = 0; i < 32; ++i)
    {
        NetGameLinkHistT& history = ref->m_stats.pinghist[i];
        history.min = 200;
        history.max = 200;
        history.avg = 200;
        history.cnt = 1;
    }

    NetCritInit(&ref->crit, "netgamelink");
    port->refptr = ref;
    port->Callback(port, NetGameLinkNotify);
    ref->m_sendenabled = 1;
    return ref;
}

// ARTIST 0x82B1DB58
extern "C" void NetGameLinkDestroy(NetGameLinkRefT* ref)
{
    ref->m_port->Callback(ref->m_port, 0);
    if (ref->m_portowner)
        ref->m_port->Destroy(ref->m_port);
    DirtyMemFree(ref->m_buffer, NETGAMELINK_MEMID, ref->m_memgroup);
    NetCritKill(&ref->crit);
    DirtyMemFree(ref, NETGAMELINK_MEMID, ref->m_memgroup);
}

// ARTIST 0x82B1DC90.'''),
])
git('diff','--check')
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
for path in (source,'vendor/dirtysdk/src/x360/connapi_x360.cpp','vendor/dirtysdk/src/x360/dirtylib_x360.cpp'):
    args+=['--source',path]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp'
tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
