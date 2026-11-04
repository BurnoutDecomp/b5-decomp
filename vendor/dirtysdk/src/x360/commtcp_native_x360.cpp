// DirtySDK native CommTCP, ARTIST 82B3BF88..82B3CD10.
#include "commall_x360_internal.h"
#include "dirtylib_x360_internal.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include <xtl.h>
#include <stddef.h>
#include <string.h>
struct CommTCPRef {
    CommRef common;
    CommTCPRef* link; SocketT* socket; sockaddr peeraddr;
    s32 state; u32 connident;
    s32 rcvoff, rcvwid, rcvlen, rcvinp, rcvout; char* rcvbuf; u32 rcvseq;
    s32 sndoff, sndwid, sndlen, sndinp, sndout; char* sndbuf; u32 sndseq;
    u32 sendtick, recvtick, dnsid; char dnsquery[256]; char* dnsbuf; s32 dnslen; char dnsdiv;
    NetCritT crit; volatile s32 callback; s32 gotevent; void (*callproc)(CommRef*,s32);
};
struct CommTCPPacket { u32 when; s16 len; char data[1]; };
CGS_STATIC_ASSERT((sizeof(CommRef)==124), "CommRef size");
CGS_STATIC_ASSERT((sizeof(CommTCPRef)==544), "CommTCPRef size");
CGS_STATIC_ASSERT((offsetof(CommTCPRef,state)==148), "CommTCP state");
CGS_STATIC_ASSERT((offsetof(CommTCPRef,rcvbuf)==176), "CommTCP receive buffer");
CGS_STATIC_ASSERT((offsetof(CommTCPRef,sndbuf)==204), "CommTCP send buffer");
CGS_STATIC_ASSERT((offsetof(CommTCPRef,crit)==492), "CommTCP critical section");
CGS_STATIC_ASSERT((offsetof(CommTCPRef,callproc)==540), "CommTCP callback");
extern "C" {
SocketT* SocketOpen(s32,s32,s32);
s32 SocketClose(SocketT*);
s32 SocketBind(SocketT*,const sockaddr*,s32);
s32 SocketListen(SocketT*,s32);
s32 SocketConnect(SocketT*,const sockaddr*,s32);
SocketT* SocketAccept(SocketT*,sockaddr*,s32*);
s32 SocketInfo(SocketT*,s32,s32,void*,s32);
s32 SocketCallback(SocketT*,s32,s32,void*,s32 (*)(SocketT*,s32,void*));
s32 SocketRecvfrom(SocketT*,char*,s32,s32,sockaddr*,s32*);
s32 SocketSendto(SocketT*,const char*,s32,s32,const sockaddr*,s32);
s32 SockaddrInParse(sockaddr*,const char*); // ARTIST 82B2D718; shared provider
s32 CommTCPStatus(CommRef*); // shared native body at 82B3FA80, in CommUDP
u32 CommUDPTick(void*); // native body 82B2A960 returns NetTick
}
static const s32 kCommTCPMem=0x63746370;
static CommTCPRef* Tcp(CommRef* p) { return reinterpret_cast<CommTCPRef*>(p); }
static s32 CommTCPEvent(SocketT*,s32,void*);
static void CommTCPSetAddrInfo(CommTCPRef* ref)
{
    sockaddr addr; u8* b=(u8*)&addr;
    SocketInfo(ref->socket,0x70656572,0,&addr,16);
    ref->common.peerip=((u32)b[4]<<24)|((u32)b[5]<<16)|((u32)b[6]<<8)|b[7];
    ref->common.hostport=(u16)(((u32)b[2]<<8)|b[3]);
    SocketInfo(ref->socket,0x62696e64,0,&addr,16);
    ref->common.hostport=(u16)(((u32)b[2]<<8)|b[3]); // original 82B3C04C store
    ref->common.hostip=((u32)b[4]<<24)|((u32)b[5]<<16)|((u32)b[6]<<8)|b[7];
    NetPrintfCode("commtcp: peer=0x%08x:%d, host=0x%08x:%d\n",ref->common.peerip,ref->common.peerport,ref->common.hostip,ref->common.hostport);
}
static s32 CommTCPEventExclusive(CommTCPRef* ref,u32 tick)
{
    s32 count=0;
    if (ref->state==2 && SocketInfo(ref->socket,0x73746174,0,0,0)>0) { ref->state=4; CommTCPSetAddrInfo(ref); }
    if (ref->state==3) {
        sockaddr addr; s32 size=16; SocketT* accepted=SocketAccept(ref->socket,&addr,&size);
        if (accepted) {
            SocketClose(ref->socket); ref->socket=accepted; ref->common.sockptr=accepted;
            SocketCallback(accepted,2,100,ref,CommTCPEvent); ref->state=4; CommTCPSetAddrInfo(ref);
        }
    }
    while (ref->state==4) {
        if ((ref->rcvwid+ref->rcvinp)%ref->rcvlen==ref->rcvout) break;
        CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->rcvbuf+ref->rcvinp);
        s32 want=ref->rcvoff<2?2:packet->len+2;
        s32 got=SocketRecvfrom(ref->socket,reinterpret_cast<char*>(&packet->len)+ref->rcvoff,want-ref->rcvoff,0,0,0);
        if (!got) break;
        if (got<0) { ref->state=5; break; }
        u32 now=NetTick(); ref->recvtick=now;
        if (!ref->rcvoff) packet->when=now;
        count+=got; ref->rcvoff+=got;
        if (ref->rcvoff>=2 && ref->rcvoff>=packet->len+2) {
            ref->rcvoff=0;
            if (packet->len) {
                ++ref->callback; ref->rcvinp=(ref->rcvwid+ref->rcvinp)%ref->rcvlen;
                if (ref->common.RecvCallback) ref->common.RecvCallback(&ref->common,packet->data,packet->len,packet->when);
                --ref->callback; ref->gotevent|=1;
            }
        }
    }
    if (ref->state==4 && ref->sndoff<0 && ref->sndinp==ref->sndout && NetTick()>ref->sendtick+50) {
        CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->sndbuf+ref->sndinp);
        packet->when=NetTick(); packet->len=0; ref->sndinp=(ref->sndwid+ref->sndinp)%ref->sndlen;
    }
    while (ref->state==4) {
        if (ref->sndoff<0) {
            if (ref->sndinp==ref->sndout) break;
            CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->sndbuf+ref->sndout);
            ref->sndoff=0;
            if (ref->common.SendCallback) ref->common.SendCallback(&ref->common,packet->data,packet->len,0);
        }
        CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->sndbuf+ref->sndout);
        s32 sent=SocketSendto(ref->socket,reinterpret_cast<const char*>(&packet->len)+ref->sndoff,packet->len-ref->sndoff+2,0,0,0);
        if (sent<=0) break;
        ref->sendtick=NetTick(); ref->sndoff+=sent;
        if (ref->sndoff==packet->len+2) { ref->sndoff=-1; ref->sndout=(ref->sndwid+ref->sndout)%ref->sndlen; }
    }
    if (!ref->callback && ref->gotevent) {
        ++ref->callback;
        if (ref->callproc) ref->callproc(&ref->common,ref->gotevent);
        ref->gotevent=0; --ref->callback;
    }
    return count;
}
static s32 CommTCPEvent(SocketT*,s32,void* p)
{
    CommTCPRef* ref=(CommTCPRef*)p;
    if (NetCritTry(&ref->crit)) { u32 tick=NetTick(); CommTCPEventExclusive(ref,tick); NetCritLeave(&ref->crit); }
    return 0;
}
extern "C" void CommTCPDestroy(CommRef* common)
{
    CommTCPRef* ref=Tcp(common);
    if (ref->state==4) { ref->connident=0; ref->state=5; }
    if (ref->socket) { SocketClose(ref->socket); ref->socket=0; ref->common.sockptr=0; }
    NetCritKill(&ref->crit); DirtyMemFree(ref->rcvbuf,kCommTCPMem,common->memgroup);
    DirtyMemFree(ref->sndbuf,kCommTCPMem,common->memgroup); DirtyMemFree(ref,kCommTCPMem,common->memgroup);
}
extern "C" void CommTCPCallback(CommRef* common,void (*callback)(CommRef*,s32)) { CommTCPRef* ref=Tcp(common); ref->callproc=callback; ref->gotevent|=2; }
extern "C" s32 CommTCPResolve(CommRef*,const char*,char*,s32,char) { NetPrintfCode("commtcp: resolve functionality not supported\n"); return -1; }
// ARTIST's Unresolve slot points to the shared return-only instruction 82AD5078.
static void CommTCPUnresolve(CommRef*) {}
static s32 CommTCPClose(CommRef* common)
{
    CommTCPRef* ref=Tcp(common);
    if (ref->state==3) { ref->socket=0; common->sockptr=0; }
    if (ref->socket) { SocketClose(ref->socket); ref->socket=0; common->sockptr=0; }
    ref->state=1; return 0;
}
extern "C" s32 CommTCPUnlisten(CommRef* common) { return CommTCPClose(common); }
extern "C" s32 CommTCPUnconnect(CommRef* common) { return CommTCPClose(common); }
extern "C" s32 CommTCPSend(CommRef* common,const void* buffer,s32 length,u32)
{
    CommTCPRef* ref=Tcp(common);
    if (ref->state!=4) return -2;
    if ((ref->sndwid+ref->sndinp)%ref->sndlen==ref->sndout) { NetPrintfCode("TCP: output buffer full\n"); return 0; }
    if (length>ref->sndwid-8) { NetPrintfCode("CommTCP: Oversized packet send (%d bytes)\n",length); return -6; }
    if (!length) return ((ref->sndlen-ref->sndout+ref->sndinp)%ref->sndlen)/ref->sndwid+1;
    CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->sndbuf+ref->sndinp);
    packet->len=(s16)length; memcpy(packet->data,buffer,length); packet->when=NetTick();
    ref->sndinp=(ref->sndwid+ref->sndinp)%ref->sndlen; CommTCPEvent(ref->socket,0,ref);
    s32 queued=((ref->sndlen-ref->sndout+ref->sndinp)%ref->sndlen)/ref->sndwid;
    return queued>0?queued:1;
}
extern "C" s32 CommTCPPeek(CommRef* common,void* data,s32 length,u32* tick)
{
    CommTCPRef* ref=Tcp(common); if (ref->rcvout==ref->rcvinp) return -7;
    CommTCPPacket* packet=reinterpret_cast<CommTCPPacket*>(ref->rcvbuf+ref->rcvout);
    if (packet->len<length) length=packet->len;
    memcpy(data,packet->data,length); if (tick) *tick=packet->when; return packet->len;
}
extern "C" s32 CommTCPRecv(CommRef* common,void* data,s32 length,u32* tick)
{
    CommTCPRef* ref=Tcp(common); s32 result=CommTCPPeek(common,data,length,tick);
    if (result>=0) ref->rcvout=(ref->rcvwid+ref->rcvout)%ref->rcvlen;
    return result;
}
static void CommTCPResetTransfer(CommTCPRef* ref)
{
    ref->sndinp=ref->sndout=ref->rcvinp=ref->rcvout=ref->rcvoff=0;
    ref->sndoff=-1; ref->sendtick=NetTick()-5000;
}
extern "C" s32 CommTCPListen(CommRef* common,const char* text)
{
    CommTCPRef* ref=Tcp(common); if (ref->state!=1 || ref->socket) return -2;
    sockaddr addr; memset(&addr,0,sizeof(addr)); addr.sa_family=2;
    if (!(SockaddrInParse(&addr,text)&2)) return -3;
    CommTCPResetTransfer(ref); ref->socket=SocketOpen(2,1,0); common->sockptr=ref->socket;
    if (!ref->socket) return -4;
    if (SocketBind(ref->socket,&addr,16)<0) { SocketClose(ref->socket); ref->socket=0; common->sockptr=0; return -5; }
    SocketListen(ref->socket,5); SocketCallback(ref->socket,2,100,ref,CommTCPEvent); ref->state=3; return 0;
}
extern "C" s32 CommTCPConnect(CommRef* common,const char* text)
{
    CommTCPRef* ref=Tcp(common); if (ref->state!=1 || ref->socket) return -2;
    memset(&ref->peeraddr,0,sizeof(ref->peeraddr)); ref->peeraddr.sa_family=2;
    if (SockaddrInParse(&ref->peeraddr,text)!=3) return -3;
    CommTCPResetTransfer(ref); ref->socket=SocketOpen(2,1,0); common->sockptr=ref->socket;
    if (!ref->socket) return -4;
    SocketConnect(ref->socket,&ref->peeraddr,16); SocketCallback(ref->socket,2,100,ref,CommTCPEvent); ref->state=2; return 0;
}
extern "C" CommTCPRef* CommTCPConstruct(s32,s32,s32);
static CommRef* CommTCPConstructForCommRef(s32 width,s32 input,s32 output)
{
    CommTCPRef* ref=CommTCPConstruct(width,input,output); return ref?&ref->common:0;
}
static u32 CommTCPTick(CommRef* common) { return CommUDPTick(common); }
extern "C" CommTCPRef* CommTCPConstruct(s32 width,s32 input,s32 output)
{
    s32 group=DirtyMemGroupQuery(); CommTCPRef* ref=(CommTCPRef*)DirtyMemAlloc(544,kCommTCPMem,group);
    if (!ref) return 0;
    memset(ref,0,544); CommRef& c=ref->common;
    c.memgroup=group; c.maxwid=(u16)width; c.maxinp=(u8)input; c.maxout=(u8)output;
    c.Construct=CommTCPConstructForCommRef; c.Destroy=CommTCPDestroy; c.Resolve=CommTCPResolve; c.Unresolve=CommTCPUnresolve;
    c.Listen=CommTCPListen; c.Unlisten=CommTCPUnlisten; c.Connect=CommTCPConnect; c.Unconnect=CommTCPUnconnect;
    c.Callback=CommTCPCallback; c.Status=CommTCPStatus; c.Tick=CommTCPTick; c.Send=CommTCPSend; c.Peek=CommTCPPeek; c.Recv=CommTCPRecv;
    NetCritInit(&ref->crit,"commtcp"); s32 stride=(width+11)&0x7ffc;
    ref->rcvwid=stride; ref->rcvlen=stride*input; ref->rcvbuf=(char*)DirtyMemAlloc(ref->rcvlen,kCommTCPMem,group);
    ref->sndwid=stride; ref->sndlen=stride*output; ref->sndbuf=(char*)DirtyMemAlloc(ref->sndlen,kCommTCPMem,group);
    ref->socket=0; c.sockptr=0; ref->state=1; ref->connident=0; return ref;
}
