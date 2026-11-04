// DirtySDK 5.5.3 common NetConn helpers, recovered from ARTIST PPC.
#include "netconn.h"
#include "dirtylib.h"

extern "C" u32 NetTick(void);

namespace {
struct IdleEntry
{
    NetConnIdleProcT* proc;
    void* data;
};
IdleEntry gIdle[32];
u32 gNextIdleTick;
u8 gMac[16];
char gMacString[14];
const char gHex[] = "0123456789abcdef";
}

// ARTIST 0x82B1DFB8
extern "C" s32 NetConnIdleAdd(NetConnIdleProcT* proc, void* data)
{
    for (u32 i = 0; i < 32; ++i)
    {
        if (gIdle[i].proc == proc && gIdle[i].data == data)
        {
            NetPrintfCode("netconn: ignoring add of an idle handler that is already registered\n");
            return -1;
        }
        if (!gIdle[i].proc)
        {
            gIdle[i].proc = proc;
            gIdle[i].data = data;
            return 0;
        }
    }
    NetPrintfCode("netconn: unable to add new idle handler as table is full\n");
    return -2;
}

// ARTIST 0x82B1E060
extern "C" s32 NetConnIdleDel(NetConnIdleProcT* proc, void* data)
{
    for (u32 i = 0; i < 32; ++i)
    {
        if (gIdle[i].proc == proc && gIdle[i].data == data)
        {
            gIdle[i].proc = 0;
            gIdle[i].data = 0;
            return 0;
        }
    }
    NetPrintfCode("netconn: ignoring delete of an idle handler that is not registered\n");
    return -1;
}

// ARTIST 0x82B1E0E8
extern "C" void NetConnIdle(void)
{
    if (NetConnStatus(0x6f70656e, 0, 0, 0))
    {
        const u32 tick = NetTick();
        if (tick >= gNextIdleTick)
        {
            gNextIdleTick = tick + 5;
            for (u32 i = 0; i < 32; ++i)
                if (gIdle[i].proc)
                    gIdle[i].proc(gIdle[i].data, tick);
        }
    }
}

// ARTIST 0x82B1E170
extern "C" void NetConnIdleShutdown(void)
{
    for (u32 i = 0; i < 32; ++i)
    {
        if (gIdle[i].proc)
        {
            NetPrintfCode("netconn: removing idle handler at shutdown\n");
            gIdle[i].proc = 0;
            gIdle[i].data = 0;
        }
    }
    gNextIdleTick = 0;
}

// ARTIST 0x82B1DEA8
extern "C" const char* NetConnMAC(void)
{
    if (!gMacString[0] && NetConnStatus(0x6d616378, 0, gMac, 16) > 0)
    {
        gMacString[0] = '$';
        gMacString[13] = 0;
        for (u32 i = 0; i < 6; ++i)
        {
            gMacString[1 + i * 2] = gHex[gMac[i] >> 4];
            gMacString[2 + i * 2] = gHex[gMac[i] & 15];
        }
    }
    return gMacString;
}
