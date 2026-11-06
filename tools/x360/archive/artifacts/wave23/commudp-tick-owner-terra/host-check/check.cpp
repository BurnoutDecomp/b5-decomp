#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "types.hpp"
#include "xtl.h"
struct SocketT { sockaddr bound; bool closed; };
#include "commudp_native_x360.cpp"
static u32 nowTick=10000; static int sends=0; static u32 lastSeq=0,lastAck=0;
static bool packetReady=false; static u8 frame[128]; static int frameLength=0; static sockaddr packetFrom;
extern "C" void* DirtyMemAlloc(s32 n,s32,s32){return std::malloc(n);}
extern "C" void DirtyMemFree(void* p,s32,s32){std::free(p);}
extern "C" s32 DirtyMemGroupQuery(){return 7;}
extern "C" void NetCritInit(NetCritT*,const char*){}
extern "C" void NetCritKill(NetCritT*){}
extern "C" s32 NetCritTry(NetCritT*){return 1;}
extern "C" void NetCritEnter(NetCritT*){}
extern "C" void NetCritLeave(NetCritT*){}
extern "C" s32 NetPrintfCode(const char*,...){return 0;}
extern "C" u32 NetTick(){return ++nowTick;}

extern "C" s32 SocketClose(SocketT* s){s->closed=true;return 0;}
extern "C" s32 SocketSendto(SocketT*,const char* p,s32 n,s32,const sockaddr*,s32){++sends;memcpy(&lastSeq,p,4);memcpy(&lastAck,p+4,4);return n;}
extern "C" s32 SocketRecvfrom(SocketT*,char* out,s32,s32,sockaddr* from,s32* len){if(!packetReady)return -1;packetReady=false;memcpy(out,frame,frameLength);*from=packetFrom;*len=16;return frameLength;}
extern "C" SocketT* SocketOpen(s32,s32,s32){return new SocketT();}
extern "C" s32 SocketBind(SocketT* s,const sockaddr* a,s32){s->bound=*a;return 0;}
extern "C" s32 SocketInfo(SocketT* s,s32 select,s32,void* out,s32){if(select==0x6d617870)return 1264;if(s&&out){*static_cast<sockaddr*>(out)=s->bound;return 0;}return -1;}
extern "C" s32 SocketCallback(SocketT*,s32,s32,void*,s32 (*)(SocketT*,s32,void*)){return 0;}
extern "C" u32 SocketHtonl(u32 x){return x;}
extern "C" u32 SocketNtohl(u32 x){return x;}
extern "C" s32 NetHash(const char*){return 123;}
extern "C" u32 XNetGetTitleXnAddr(XNADDR*){return 0;}
static void queue(u32 seq,u32 ack,u32 client,u32 remote,const char* data,int n){memcpy(frame,&seq,4);memcpy(frame+4,&ack,4);memcpy(frame+8,&client,4);memcpy(frame+12,&remote,4);if(n)memcpy(frame+16,data,n);frameLength=16+n;packetReady=true;}
int main(){
 CommUDPRef* r=CommUDPConstruct(32,8,4); if(!r)return 1;
 u32 expected=nowTick+1; if(r->common.Tick(&r->common)!=expected || nowTick!=expected) return 8; expected=nowTick+1; if(CommUDPTick(0)!=expected || nowTick!=expected) return 9;
 SocketT* s=SocketOpen(2,2,0);r->socket=r->common.sockptr=s; r->state=4;r->recvSequence=256;r->recvUnreliable=128;r->clientId=7;r->remoteId=9;
 CommUDPSetAddress(&r->peer,0x0a010203,1234); packetFrom=r->peer; r->sendTick=r->recvTick=nowTick;
 queue(256,0,9,7,"data",4); if(CommUDPPumpNative(nowTick)!=1 || r->recvSequence!=257 || r->recvHead!=56 || r->common.packrcvd!=1) return 2;
 char out[8]={};u32 stamp=0;if(CommUDPRecv(&r->common,out,8,&stamp)!=4 || memcmp(out,"data",4) || r->recvTail!=56) return 3;
 const int oldSends=sends;queue(258,0,9,7,"late",4);if(CommUDPPumpNative(nowTick)!=1 || r->recvSequence!=257 || r->common.packlost!=1 || sends!=oldSends+1 || lastSeq!=4 || lastAck!=257) return 4;
 queue(257,0,9,7,"next",4);if(CommUDPPumpNative(nowTick)!=1 || r->recvSequence!=258) return 5;
 CommUDPPacket* q=reinterpret_cast<CommUDPPacket*>(r->sendBuffer);q->length=1;q->sequence=256; r->sendTail=0;r->sendHead=56;r->sendNext=56;
 queue(4,257,9,7,0,0); if(CommUDPPumpNative(nowTick)!=1 || r->sendTail!=56 || r->sendNext!=56) return 6;
 r->state=2;r->sendTick=nowTick-1100;const int before=sends; if(CommUDPPumpNative(nowTick)!=0 || sends!=before+1 || lastSeq!=1 || lastAck!=r->connectionId) return 7;
 r->sendTail=r->sendNext=r->sendHead=0; CommUDPDestroy(&r->common);
 puts("commudp pump host checks passed (mocked socket and clock)");return 0;
}

