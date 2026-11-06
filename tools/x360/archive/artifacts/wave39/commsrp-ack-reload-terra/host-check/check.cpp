#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "xtl.h"
struct SocketT { int x; };
#include "commsrp_native_x360.cpp"
static u32 clockNow=10000; static int sent=0; static u8 sentCode=0; static u8 incoming[128]; static int inLength=0; static SocketT sock;
extern "C" void* DirtyMemAlloc(s32 n,s32,s32){return std::malloc(n);}
extern "C" void DirtyMemFree(void* p,s32,s32){std::free(p);}
extern "C" s32 DirtyMemGroupQuery(){return 7;}
extern "C" void NetCritInit(NetCritT*,const char*){}
extern "C" void NetCritKill(NetCritT*){}
extern "C" s32 NetCritTry(NetCritT*){return 1;}
extern "C" void NetCritEnter(NetCritT*){}
extern "C" void NetCritLeave(NetCritT*){}
extern "C" u32 NetTick(){return clockNow;}
extern "C" s32 NetPrintfCode(const char*,...){return 0;}
extern "C" s32 SocketClose(SocketT*){return 0;}
extern "C" SocketT* SocketOpen(s32,s32,s32){return &sock;}
extern "C" s32 SocketBind(SocketT*,const sockaddr*,s32){return 0;}
extern "C" s32 SocketCallback(SocketT*,s32,s32,void*,s32 (*)(SocketT*,s32,void*)){return 0;}
extern "C" s32 SockaddrInParse2(u32* ip,s32* port,s32* peer,const char*){*ip=0x0a010203;*port=1234;*peer=1235;return 3;}
extern "C" s32 SocketInfo(SocketT*,s32,s32,void* out,s32){memset(out,0,16);return 0;}
extern "C" u32 SocketGetLocalAddr(){return 0;}
extern "C" s32 SocketSendto(SocketT*,const char* p,s32 n,s32,const sockaddr*,s32){sentCode=(u8)p[0];++sent;return n;}
extern "C" s32 SocketRecvfrom(SocketT*,char* out,s32,s32,sockaddr* from,s32* len){if(!inLength)return 0;int n=inLength;inLength=0;memcpy(out,incoming,n);memset(from,0,16);*len=16;return n;}
static u8 replacement; static void mutate(CommRef*,void* data,s32,u32){static_cast<u8*>(data)[-1]=replacement;}
int main(){
 CommSRPRef* r=CommSRPConstruct(32,8,4);if(!r||r->recvStride!=44||r->recvSize!=352||r->sendSize!=176||r->recvLimit!=1||r->common.memgroup!=7)return 1;
 if(r->common.Connect(&r->common,"peer")||r->state!=1||r->recvReliable!=-1)return 2;
 CommSRPPump(r);if(sent!=1||sentCode!=16)return 3;
 incoming[0]=18;inLength=1;CommSRPPump(r);if(r->state!=3)return 4;
 if(r->common.Send(&r->common,"abc",3,0)!=1||sentCode!=128||r->sendHead!=44||r->sendTail!=0)return 5;
 incoming[0]=192;inLength=1;CommSRPPump(r);if(r->sendTail!=44)return 6;
 incoming[0]=128;memcpy(incoming+1,"xyz",3);inLength=4;CommSRPPump(r);if(r->recvHead!=44||sentCode!=192)return 7;
 char out[8]={};u32 tick;if(r->common.Recv(&r->common,out,8,&tick)!=3||memcmp(out,"xyz",3)||r->recvTail!=44)return 8;
 r->common.RecvCallback=mutate;
 replacement=127;int prev=sent;incoming[0]=129;incoming[1]='a';inLength=2;CommSRPPump(r);if(sent!=prev)return 10;
 replacement=192;prev=sent;incoming[0]=130;incoming[1]='b';inLength=2;CommSRPPump(r);if(sent!=prev)return 11;
 replacement=130;prev=sent;incoming[0]=131;incoming[1]='c';inLength=2;CommSRPPump(r);if(sent!=prev+1||sentCode!=194)return 12;
 int before=sent;r->common.Destroy(&r->common);if(sent!=before+1||sentCode!=20)return 9;
 puts("SRP lifecycle, handshake, reliable send/ack, receive/ack and close checks passed (mock socket/clock)");return 0;
}

