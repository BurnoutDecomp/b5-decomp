#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "types.hpp"
#include "xtl.h"
struct SocketT { sockaddr bound; bool closed; };
#include "commudp_native_x360.cpp"
static int sends=0; static int lastSize=0; static u32 lastSeq=0,lastAck=0; static char lastPayload[64]; static u32 nowTick=10000; static int processCalls=0;
extern "C" void* DirtyMemAlloc(s32 n,s32,s32) {return std::malloc(n);}
extern "C" void DirtyMemFree(void* p,s32,s32) {std::free(p);}
extern "C" s32 DirtyMemGroupQuery(){return 7;}
extern "C" void NetCritInit(NetCritT*,const char*){}
extern "C" void NetCritKill(NetCritT*){}
extern "C" s32 NetCritTry(NetCritT*){return 1;}
extern "C" void NetCritEnter(NetCritT*){}
extern "C" void NetCritLeave(NetCritT*){}
extern "C" s32 NetPrintfCode(const char*,...){return 0;}
extern "C" u32 NetTick(){return ++nowTick;}
extern "C" u32 CommUDPTick(CommRef*){return NetTick();}
extern "C" s32 SocketClose(SocketT* s){s->closed=true;return 0;}
extern "C" s32 SocketSendto(SocketT*,const char* p,s32 n,s32,const sockaddr*,s32){++sends;lastSize=n;memcpy(&lastSeq,p,4);memcpy(&lastAck,p+4,4);if(n>16 && n<=80)memcpy(lastPayload,p+16,n-16);return n;}
extern "C" SocketT* SocketOpen(s32,s32,s32){return new SocketT();}
extern "C" s32 SocketBind(SocketT* s,const sockaddr* a,s32){s->bound=*a;return 0;}
extern "C" s32 SocketInfo(SocketT* s,s32 select,s32,void* out,s32){if(select==0x6d617870)return 1264;if(s&&out){*static_cast<sockaddr*>(out)=s->bound;return 0;}return -1;}
extern "C" s32 SocketCallback(SocketT*,s32,s32,void*,s32 (*)(SocketT*,s32,void*)){return 0;}
extern "C" s32 CommUDPPumpNative(u32){++processCalls;return 0;}
extern "C" u32 SocketHtonl(u32 x){return x;}
extern "C" u32 SocketNtohl(u32 x){return x;}
extern "C" s32 NetHash(const char*){return 0x12345678;}
extern "C" u32 XNetGetTitleXnAddr(XNADDR*){return 0;}
int main(){
 u32 address=0; s32 local=0,peer=0;
 if(SockaddrInParse2(&address,&local,&peer," 10.1.2.3:1500:1501#x")!=7 || address!=0x0a010203 || local!=1500 || peer!=1501) return 1;
 CommUDPRef* a=CommUDPConstruct(32,4,4); if(!a || a->state!=1 || a->recvStride!=56 || a->sendSize!=224) return 2;
 if(CommUDPListen(&a->common,"10.1.2.3:1500:1501#x") || a->state!=3 || a->connectionId!=0x12345678) return 3;
 if(CommUDPUnlisten(&a->common) || a->state!=1) return 4;
 if(CommUDPConnect(&a->common,"10.1.2.3:1500:1501#x") || a->state!=2) return 5;
 a->state=4; const char p[]="abcd";
 if(CommUDPSend(&a->common,p,4,0)!=1 || a->sendHead!=56 || a->sendNext!=56 || sends!=1 || lastSize!=20 || lastSeq!=256 || lastAck!=255 || memcmp(lastPayload,p,4)) return 6;
 if(CommUDPSend(&a->common,p,4,0)!=2 || a->sendHead!=112 || a->sendNext!=56 || sends!=1) return 7;
 if(CommUDPSend(&a->common,0,0,0)!=3) return 8;
 if(CommUDPSend(&a->common,p,4,1)!=1 || a->sendUnreliable!=129 || sends!=2) return 9;
 if(CommUDPSend(&a->common,p,33,0)!=-6) return 10;
 CommUDPDestroy(&a->common);
 puts("commudp host checks passed (mocked transport)"); return 0;
}
