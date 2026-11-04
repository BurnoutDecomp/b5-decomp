#ifndef DIRTYSDK_COMMALL_X360_INTERNAL_H
#define DIRTYSDK_COMMALL_X360_INTERNAL_H
#include "types.hpp"
struct SocketT;
// DirtySDK commall.h, DecFIGS declaration shape; ARTIST NetGameLinkStatus
// reads Status and packsent/packrcvd/packlost at 0x24 and 0x60..0x68.
struct CommRef
{
    CommRef* (*Construct)(s32, s32, s32);
    void (*Destroy)(CommRef*);
    s32 (*Resolve)(CommRef*, const char*, char*, s32, char);
    void (*Unresolve)(CommRef*);
    s32 (*Listen)(CommRef*, const char*);
    s32 (*Unlisten)(CommRef*);
    s32 (*Connect)(CommRef*, const char*);
    s32 (*Unconnect)(CommRef*);
    void (*Callback)(CommRef*, void (*)(CommRef*, s32));
    s32 (*Status)(CommRef*);
    s32 (*Control)(CommRef*, s32, s32, void*);
    u32 (*Tick)(CommRef*);
    s32 (*Send)(CommRef*, const void*, s32, u32);
    s32 (*Peek)(CommRef*, void*, s32, u32*);
    s32 (*Recv)(CommRef*, void*, s32, u32*);
    void (*SendCallback)(CommRef*, const void*, s32, u32);
    void (*RecvCallback)(CommRef*, void*, s32, u32);
    s32 memgroup, refnum;
    void* refptr;
    SocketT* sockptr;
    u16 maxwid;
    u8 maxinp, maxout;
    s32 datasent, datarcvd, packsent, packrcvd, packlost, overhead;
    u32 hostip, peerip;
    u16 hostport, peerport;
};
#endif
