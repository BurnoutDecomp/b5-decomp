// Native DirtySDK 5.5.3 tunnel lifecycle. ARTIST 82B2E120, 82B2E190,
// 82B2E468, 82B2E8B0, 82B2EA10, 82B2F368 and 82B2F4A0.
#include "prototunnel.h"
#include "cryptarc4.h"
#include "dirtymem.h"
#include "dirtylib.h"
#include "dirtylib_x360_internal.h"
#include <stddef.h>
#include <string.h>

struct SocketT;
struct ProtoTunnelStatT { u32 uNumBytes, uNumSubpacketBytes; u16 uNumPackets, uNumSubpackets; };
struct ProtoTunnelT
{
    ProtoTunnelInfoT Info;
    u32 uVirtualAddr;
    s16 iDataSize;
    s8 iNumPackets;
    NetCritT PacketCrit;
    u16 uSendOffset, uRecvOffset;
    CryptArc4T CryptSendState, CryptRecvState;
    u32 uLastStatUpdate;
    ProtoTunnelStatT LastSendStat, SendStat, RecvStat;
    char aKeyList[8][128];
    u8 uActive, uRefCount, uSendKey, uRecvKey, bSendCtrlInfo;
    u8 aPacketData[1264];
};
struct ProtoTunnelRefT
{
    s32 iMemGroup;
    SocketT* pSocket;
    u32 uTunnelPort, uLocalClientId;
    s8 iNumTunnels, iMaxTunnels, iVerbosity;
    s32 eMode;
    u32 uVirtualAddr, uFlushRate, uLastFlush;
    s32 iPacketDrop;
    void (*pCallback)(ProtoTunnelRefT*,s32,const char*,s32,sockaddr*,void*);
    void* pUserData;
    NetCritT TunnelsCritS, TunnelsCritR;
    ProtoTunnelT Tunnels[1];
};
CGS_STATIC_ASSERT((sizeof(ProtoTunnelInfoT)==36), "tunnel info size");
CGS_STATIC_ASSERT((sizeof(ProtoTunnelT)==2940), "tunnel size");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelT,PacketCrit)==44), "tunnel lock");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelT,CryptSendState)==88), "tunnel cipher");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelT,aKeyList)==644), "tunnel keys");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelT,aPacketData)==1673), "tunnel packet");
CGS_STATIC_ASSERT((offsetof(ProtoTunnelRefT,Tunnels)==128), "tunnel array");
extern "C" {
SocketT* SocketOpen(s32,s32,s32);
s32 SocketBind(SocketT*,const sockaddr*,s32);
s32 SocketInfo(SocketT*,s32,s32,void*,s32);
s32 SocketClose(SocketT*);
s32 SocketCallback(SocketT*,s32,s32,void*,s32 (*)(SocketT*,s32,void*));
s32 SocketControl(SocketT*,s32,s32,void*,void*);
s32 SocketSendto(SocketT*,const char*,s32,s32,const sockaddr*,s32);
s32 SocketRecvfrom(SocketT*,char*,s32,s32,sockaddr*,s32*);
u16 SocketHtons(u16);
void NetPrintMemCode(const void*,s32,const char*);
const char* SocketInAddrGetText(u32);
char* ds_strnzcpy(char*,const char*,s32);
// Native packet-processing functions, recovered from ARTIST.
s32 ProtoTunnelRecvCallback(SocketT*,s32,void*); // 82B2F078
s32 ProtoTunnelSendCallback(SocketT*,s32,const u8*,s32,const sockaddr*,void*); // 82B2DDD0
s32 NetPrintfVerboseCode(s32,s32,const char*,...);
void ProtoTunnelBufferSend(ProtoTunnelRefT*,ProtoTunnelT*); // 82B2D878
}
// ARTIST 82B2E6D0. The original accepts the caller's packet length without
// checking it against this local buffer; retain that inherited input behavior.
extern "C" s32 ProtoTunnelValidatePacket(ProtoTunnelRefT* pRef,ProtoTunnelT* pTunnel,
    u8* pOutput,const u8* pPacket,s32 iLength,const char* pKey)
{
    if (pRef->eMode) {
        if (pOutput) memcpy(pOutput,pPacket,iLength);
        s32 packetLength=iLength-2, decoded=0, count=0;
        const u8* pHeader=pPacket+2;
        while (decoded<packetLength) {
            s32 len=((u32)pHeader[0]<<4)|(pHeader[1]>>4);
            ++count; pHeader+=2; decoded+=len+2;
        }
        if (decoded!=packetLength) {
            NetPrintf(("prototunnel: [recv] mismatched size in received packet (%d received, %d decoded)\n",packetLength,decoded));
            return -1;
        }
        return count;
    }
    u8 packet[1264]; memcpy(packet,pPacket,iLength);
    u32 offset=((u32)packet[0]<<8)|packet[1];
    CryptArc4T cipher; CryptArc4Init(&cipher,(const u8*)pKey,(s32)strlen(pKey),1);
    if (offset) { NetPrintf(("prototunnel: stream skip %d bytes\n",offset*4)); CryptArc4Advance(&cipher,offset*4); }
    u8* pHeader=packet+2;
    s32 decoded=2, count=0, encryptedData=0;
    while (decoded<iLength) {
        CryptArc4Apply(&cipher,pHeader,2);
        u32 len=((u32)pHeader[0]<<4)|(pHeader[1]>>4);
        if (pTunnel->Info.aPortFlags[pHeader[1]&15]&1) encryptedData+=len;
        decoded+=len+2; pHeader+=2; ++count;
    }
    if (decoded!=iLength) {
        NetPrintf(("prototunnel: [recv] mismatched size in received packet (%d received, %d decoded)\n",iLength,decoded));
        return -1;
    }
    if (pOutput) {
        if (encryptedData>0) CryptArc4Apply(&cipher,pHeader,encryptedData);
        memcpy(pOutput,packet,iLength);
    }
    return count;
}
// ARTIST 82B2DF50: trial decrypt and commit the stream only on packet match.
extern "C" s32 ProtoTunnelDecryptPacket(ProtoTunnelT* pTunnel,u8* pPacket,s32 iLength)
{
    u8 saved[36]; memcpy(saved,pPacket,sizeof(saved));
    s32 offset=((u32)pPacket[0]<<8)|pPacket[1];
    CryptArc4T cipher; memcpy(&cipher,&pTunnel->CryptRecvState,sizeof(cipher));
    if (offset!=pTunnel->uRecvOffset) {
        s32 diff=offset-pTunnel->uRecvOffset;
        if (diff<0) {
            if (diff>=-16384) { NetPrintf(("prototunnel: received out of order packet (off=%d); discarding\n",diff)); return 0; }
            NetPrintf(("prototunnel: stream window wrap in sync\n")); diff+=0x10000;
        }
        NetPrintf(("prototunnel: stream skip %d bytes\n",4*diff));
        CryptArc4Advance(&cipher,4*diff); pTunnel->uRecvOffset+=(u16)diff;
    }
    u8* pHeader=pPacket+2;
    u32 encrypted=0;
    s32 decoded=2, count=0, encryptedData=0;
    while (decoded<iLength) {
        CryptArc4Apply(&cipher,pHeader,2); encrypted+=2;
        u32 len=((u32)pHeader[0]<<4)|(pHeader[1]>>4);
        if (pTunnel->Info.aPortFlags[pHeader[1]&15]&1) encryptedData+=len;
        decoded+=len+2; pHeader+=2; ++count;
    }
    if (decoded!=iLength) {
        NetPrintf(("prototunnel: [recv] mismatched size in received packet (%d received, %d decoded)\n",iLength,decoded));
        memcpy(pPacket,saved,sizeof(saved)); return -1;
    }
    if (encryptedData>0) { CryptArc4Apply(&cipher,pHeader,encryptedData); encrypted+=encryptedData; }
    memcpy(&pTunnel->CryptRecvState,&cipher,sizeof(cipher));
    pTunnel->uRecvOffset+=(u16)(encrypted>>2);
    if (encrypted&3) { CryptArc4Advance(&pTunnel->CryptRecvState,4-(encrypted&3)); ++pTunnel->uRecvOffset; }
    return count;
}
// ARTIST 82B2EAA8: identify the key and control packet for a tunnel.
static s32 ProtoTunnelMatch(ProtoTunnelRefT* pRef,ProtoTunnelT* pTunnel,const u8* pPacket,s32 iLength)
{
    u8 packet[1264]; s32 key=0, count=0;
    for (;key<8;++key) if (pTunnel->aKeyList[key][0]) {
        count=ProtoTunnelValidatePacket(pRef,pTunnel,packet,pPacket,iLength,pTunnel->aKeyList[key]);
        if (count>0) break;
    }
    if (key==8) return 0;
    if (pTunnel->uActive) {
        NetPrintf(("prototunnel: successful rematch to key %d (%s)\n",key,pTunnel->aKeyList[key]));
        CryptArc4Init(&pTunnel->CryptRecvState,(const u8*)pTunnel->aKeyList[key],(s32)strlen(pTunnel->aKeyList[key]),1);
        pTunnel->uRecvKey=(u8)key; pTunnel->uRecvOffset=0; return count;
    }
    u8* pHeader=packet+2; u8* pData=packet+2*(count+1); u8* pControl=0;
    for (s32 i=0;i<count;++i) {
        u32 value=((u32)pHeader[0]<<8)|pHeader[1];
        if ((value&15)==7) { pControl=pData; break; }
        pHeader+=2; pData+=value>>4;
    }
    if (pControl) {
        u32 client=((u32)pControl[1]<<24)|((u32)pControl[2]<<16)|((u32)pControl[3]<<8)|pControl[4];
        if (client==pTunnel->Info.uRemoteClientId) {
            if (key) NetPrintf(("prototunnel: matched key with index=%d\n",key));
            CryptArc4Init(&pTunnel->CryptRecvState,(const u8*)pTunnel->aKeyList[key],(s32)strlen(pTunnel->aKeyList[key]),1);
            pTunnel->uRecvKey=(u8)key; pTunnel->uRecvOffset=0; return 1;
        }
        NetPrintf(("prototunnel: packet clientId 0x%08x does not match tunnel clientId 0x%08x\n",client,pTunnel->Info.uRemoteClientId));
        if (pRef->iVerbosity>2) NetPrintMemCode(packet,iLength<64?iLength:64,"prototunnel-recv-decrypt");
    } else NetPrintf(("prototunnel: no control data included in packet on inactive tunnel\n"));
    return 0;
}
// ARTIST 82B2ECE0: match source, decrypt and account the received datagram.
static s32 ProtoTunnelRecvData(ProtoTunnelRefT* pRef,u16* pPorts,s32 iPortBytes,u8* pPacket,s32 iLength,sockaddr* pFrom)
{
    NetCritEnter(&pRef->TunnelsCritR);
    u8* a=(u8*)pFrom;
    u32 port=((u32)a[2]<<8)|a[3];
    u32 addr=((u32)a[4]<<24)|((u32)a[5]<<16)|((u32)a[6]<<8)|a[7];
    s32 i=0;
    for (;i<pRef->iNumTunnels;++i) {
        ProtoTunnelT* p=&pRef->Tunnels[i];
        if (p->Info.uRemoteAddr==addr && p->Info.uRemotePort==port &&
            (p->uActive || ProtoTunnelMatch(pRef,p,pPacket,iLength))) break;
    }
    if (i==pRef->iNumTunnels) {
        i=0;
        for (;i<pRef->iNumTunnels;++i) {
            ProtoTunnelT* p=&pRef->Tunnels[i];
            if (!p->uActive && ProtoTunnelMatch(pRef,p,pPacket,iLength)) break;
        }
    }
    if (i>=pRef->iNumTunnels) { NetCritLeave(&pRef->TunnelsCritR); return -2; }
    ProtoTunnelT* p=&pRef->Tunnels[i];
    if (!p->uActive) {
        p->Info.uRemoteAddr=addr; p->Info.uRemotePort=(u16)port; p->uActive=1;
        NetPrintf(("prototunnel: matched incoming data from %s:%d to tunnelId 0x%08x / clientId 0x%08x\n",SocketInAddrGetText(addr),port,p->uVirtualAddr,p->Info.uRemoteClientId));
    }
    memcpy(pPorts,p->Info.aRemotePortList,iPortBytes);
    NetPrintfVerboseCode(pRef->iVerbosity,1,"prototunnel: [recv] received %d bytes on tunnel 0x%08x from %s:%d\n",iLength,p->uVirtualAddr,SocketInAddrGetText(addr),port);
    s32 count;
    if (pRef->eMode) {
        s32 left=iLength-2, decoded=0; count=0; u8* h=pPacket+2;
        while (decoded<left) { s32 len=((u32)h[0]<<4)|(h[1]>>4); ++count; h+=2; decoded+=len+2; }
        if (decoded!=left) { NetPrintf(("prototunnel: [recv] mismatched size in received packet (%d received, %d decoded)\n",left,decoded)); count=-1; }
    } else count=ProtoTunnelDecryptPacket(p,pPacket,iLength);
    if (count<=0) {
        NetPrintf(("prototunnel: attempting to rematch tunnel key\n"));
        ProtoTunnelMatch(pRef,p,pPacket,iLength);
        NetCritLeave(&pRef->TunnelsCritR); return count;
    }
    p->RecvStat.uNumBytes+=iLength; p->RecvStat.uNumSubpacketBytes+=iLength-2*count-2;
    ++p->RecvStat.uNumPackets; p->RecvStat.uNumSubpackets+=(u16)count;
    u8* h=pPacket+2; u8* data=pPacket+2*count+2; u8* control=0;
    for (s32 j=0;j<count;++j) {
        u32 value=((u32)h[0]<<8)|h[1];
        if ((value&15)==7) { control=data; break; }
        h+=2; data+=value>>4;
    }
    if ((control && control[5]==1) || (!control && p->bSendCtrlInfo && p->uActive==1)) {
        NetPrintf(("prototunnel: connection is active; disabling control packets\n")); p->bSendCtrlInfo=0;
    }
    u32 virt=p->uVirtualAddr;
    a[4]=(u8)(virt>>24); a[5]=(u8)(virt>>16); a[6]=(u8)(virt>>8); a[7]=(u8)virt;
    NetCritLeave(&pRef->TunnelsCritR); return count;
}
// ARTIST 82B2F078: dispatch tunnel subpackets into the bound native sockets.
extern "C" s32 ProtoTunnelRecvCallback(SocketT* pSocket,s32,void* pUserData)
{
    ProtoTunnelRefT* pRef=(ProtoTunnelRefT*)pUserData;
    u8 packet[1328]; sockaddr from; s32 fromLen=16;
    s32 len=SocketRecvfrom(pSocket,(char*)packet,1264,0,&from,&fromLen);
    if (len<=0) return 0;
    if (pRef->iPacketDrop>0) { --pRef->iPacketDrop; NetPrintf(("prototunnel: dropping received packet\n")); return 0; }
    u16 ports[8];
    s32 count=ProtoTunnelRecvData(pRef,ports,16,packet,len,&from);
    if (count==-1) count=ProtoTunnelRecvData(pRef,ports,16,packet,len,&from);
    u8* a=(u8*)&from;
    u32 remote=((u32)a[4]<<24)|((u32)a[5]<<16)|((u32)a[6]<<8)|a[7];
    u32 port=((u32)a[3]<<8)|a[2];
    if (count==-2) {
        NetPrintf(("prototunnel: [recv] received %d byte packet on tunnel socket from unmapped source %s:%d\n",len,SocketInAddrGetText(remote),port));
        if (pRef->pCallback) pRef->pCallback(pRef,0,(const char*)packet,len,&from,pRef->pUserData);
    }
    if (count<=0) {
        NetPrintf(("prototunnel: [recv] received unhandled %d bytes from %s:%d\n",len,SocketInAddrGetText(remote),port));
        if (pRef->iVerbosity>2) NetPrintMemCode(packet,len<64?len:64,"prototunnel-recv-nomatch");
        return 0;
    }
    if (pRef->iVerbosity>2) NetPrintMemCode(packet,len<64?len:64,"prototunnel-recv");
    u8* h=packet+2; u8* data=packet+2+2*count;
    for (s32 i=0;i<count;++i) {
        u32 header=((u32)h[0]<<8)|h[1]; u32 index=header&15; u32 size=header>>4;
        u32 destPort=ports[index];
        if (index==7) NetPrintf(("prototunnel: [recv] received control packet\n"));
        else {
            SocketT* target=0;
            if (SocketInfo(0,0x62696e64,destPort,&target,4))
                NetPrintf(("prototunnel: [recv] warning, got data for port %d with no socket bound to that port\n",destPort));
            else {
                a[2]=(u8)(destPort>>8); a[3]=(u8)destPort;
                NetPrintfVerboseCode(pRef->iVerbosity,1,"prototunnel: [recv] %db->%d\n",size,destPort);
                if (SocketInfo(target,0x70726f74,0,0,0)==254) { data-=2; size+=2; }
                SocketControl(target,0x70757368,size,data,&from);
            }
        }
        h+=2; data+=size;
    }
    return 0;
}
// ARTIST 82B2D7A8: copy queued headers and payloads of one flag class.
static s32 ProtoTunnelCollect(ProtoTunnelT* pTunnel,u8** ppHeader,u8** ppData,s32 iClass)
{
    u8* pSrcHeader=pTunnel->aPacketData+2;
    u8* pSrcData=pTunnel->aPacketData+18;
    s32 copied=0;
    for (s32 i=0;i<pTunnel->iNumPackets;++i) {
        u32 len=((u32)pSrcHeader[0]<<4)|(pSrcHeader[1]>>4);
        if ((pTunnel->Info.aPortFlags[pSrcHeader[1]&15]&1)==iClass) {
            memcpy(*ppData,pSrcData,len);
            copied+=len;
            (*ppHeader)[0]=pSrcHeader[0]; (*ppHeader)[1]=pSrcHeader[1];
            *ppData+=len; *ppHeader+=2;
        }
        pSrcHeader+=2; pSrcData+=len;
    }
    return copied;
}
// ARTIST 82B2D878: assemble, optionally encrypt, account and send a datagram.
extern "C" void ProtoTunnelBufferSend(ProtoTunnelRefT* pRef,ProtoTunnelT* pTunnel)
{
    NetCritEnter(&pTunnel->PacketCrit);
    if (!pRef->pSocket || pTunnel->iDataSize<=0) { NetCritLeave(&pTunnel->PacketCrit); return; }
    sockaddr to;
    memset(&to,0,sizeof(to)); to.sa_family=2;
    to.sa_data[0]=(char)(pTunnel->Info.uRemotePort>>8);
    to.sa_data[1]=(char)pTunnel->Info.uRemotePort;
    u32 remote=pTunnel->Info.uRemoteAddr;
    to.sa_data[2]=(char)(remote>>24); to.sa_data[3]=(char)(remote>>16);
    to.sa_data[4]=(char)(remote>>8); to.sa_data[5]=(char)remote;
    u8 packet[1344]; memset(packet,0,1264);
    s32 headerSize=2*(pTunnel->iNumPackets+pTunnel->bSendCtrlInfo+1);
    u8* pHeader=packet+2;
    u8* pData=packet+headerSize;
    s32 encryptedData=0;
    if (pTunnel->bSendCtrlInfo) {
        u8 control[6]; control[0]=0;
        control[1]=(u8)(pRef->uLocalClientId>>24);
        control[2]=(u8)(pRef->uLocalClientId>>16);
        control[3]=(u8)(pRef->uLocalClientId>>8);
        control[4]=(u8)pRef->uLocalClientId; control[5]=pTunnel->uActive;
        memcpy(pData,control,6); packet[2]=0; packet[3]=103;
        pHeader=packet+4; pData+=6; encryptedData=6;
    }
    encryptedData+=ProtoTunnelCollect(pTunnel,&pHeader,&pData,1);
    s32 totalData=encryptedData+ProtoTunnelCollect(pTunnel,&pHeader,&pData,0);
    u32 encryptedLength=(u32)(encryptedData+headerSize-2);
    s32 packetLength=totalData+headerSize;
    if (pRef->iVerbosity>2) NetPrintMemCode(packet,packetLength<64?packetLength:64,"prototunnel-send-nocrypt");
    if (pRef->eMode) {
        u16 size=SocketHtons((u16)encryptedLength);
        packet[0]=(u8)(size>>8); packet[1]=(u8)size;
    } else {
        packet[0]=(u8)(pTunnel->uSendOffset>>8); packet[1]=(u8)pTunnel->uSendOffset;
        CryptArc4Apply(&pTunnel->CryptSendState,packet+2,encryptedLength);
        pTunnel->uSendOffset+=(u16)(encryptedLength>>2);
        if (encryptedLength&3) { CryptArc4Advance(&pTunnel->CryptSendState,4-(encryptedLength&3)); ++pTunnel->uSendOffset; }
    }
    s32 numPackets=pTunnel->iNumPackets+pTunnel->bSendCtrlInfo;
    pTunnel->iDataSize=0; pTunnel->iNumPackets=0;
    ++pTunnel->SendStat.uNumPackets;
    pTunnel->SendStat.uNumBytes+=packetLength;
    pTunnel->SendStat.uNumSubpacketBytes+=packetLength-headerSize;
    pTunnel->SendStat.uNumSubpackets+=(u16)numPackets;
    NetCritLeave(&pTunnel->PacketCrit);
    if ((s32)(NetTick()-pTunnel->uLastStatUpdate)>5000) {
        u32 bytes=pTunnel->SendStat.uNumBytes-pTunnel->LastSendStat.uNumBytes;
        u32 subBytes=pTunnel->SendStat.uNumSubpacketBytes-pTunnel->LastSendStat.uNumSubpacketBytes;
        u16 packets=(u16)(pTunnel->SendStat.uNumPackets-pTunnel->LastSendStat.uNumPackets);
        u16 subpackets=(u16)(pTunnel->SendStat.uNumSubpackets-pTunnel->LastSendStat.uNumSubpackets);
        pTunnel->LastSendStat=pTunnel->SendStat;
        float packetEfficiency=(float)subpackets/(float)packets;
        float byteEfficiency=(float)subBytes/(float)bytes;
        NetPrintfVerboseCode(pRef->iVerbosity,1,"prototunnel: pkts: %d eff: %.2f sent: %d eff: %.2f\n",packets,packetEfficiency,bytes,byteEfficiency);
        pTunnel->uLastStatUpdate=NetTick();
    }
    if (pRef->iVerbosity>2) NetPrintMemCode(packet,packetLength<64?packetLength:64,"prototunnel-send");
    s32 result=SocketSendto(pRef->pSocket,(const char*)packet,packetLength,0,&to,16);
    u32 port=((u8)to.sa_data[1]<<8)|(u8)to.sa_data[0];
    if (result<0) NetPrintf(("prototunnel: [send] error %d sending buffered packet to %s:%d\n",result,SocketInAddrGetText(remote),port));
    else NetPrintfVerboseCode(pRef->iVerbosity,1,"prototunnel: [send] sent %d bytes [%d packets] to %s:%d\n",packetLength,numPackets,SocketInAddrGetText(remote),port);
}
// ARTIST 82B2DCC8: queue the datagram in the tunnel packet buffer.
// The nested flush takes PacketCrit recursively, as in the native implementation.
static s32 ProtoTunnelBufferPacket(ProtoTunnelRefT* pRef, ProtoTunnelT* pTunnel,
                                  const u8* pData, s32 iLength, s32 iPortIndex)
{
    u32 length=(u32)iLength;
    if (!length || length>=0x4ec) {
        NetPrintf(("prototunnel: packet of size %d will not be tunneled\n",iLength));
        return 0;
    }
    NetCritEnter(&pTunnel->PacketCrit);
    if (pTunnel->iDataSize+length>0x4f0 || pTunnel->iNumPackets==8)
        ProtoTunnelBufferSend(pRef,pTunnel);
    if (!pTunnel->iDataSize) {
        pTunnel->iDataSize=18;
        memset(pTunnel->aPacketData,0,18);
    }
    s32 index=2*pTunnel->iNumPackets+2;
    pTunnel->aPacketData[index]=(u8)(length>>4);
    pTunnel->aPacketData[index+1]=(u8)((length<<4)|iPortIndex);
    memcpy(pTunnel->aPacketData+pTunnel->iDataSize,pData,length);
    pTunnel->iDataSize=(s16)(pTunnel->iDataSize+length);
    ++pTunnel->iNumPackets;
    NetCritLeave(&pTunnel->PacketCrit);
    return (s32)length;
}
// ARTIST 82B2DDD0, callback registered through SocketControl('sdcb').
extern "C" s32 ProtoTunnelSendCallback(SocketT* pSocket,s32 iType,const u8* pData,
                                       s32 iLength,const sockaddr* pTo,void* pUserData)
{
    ProtoTunnelRefT* pRef=(ProtoTunnelRefT*)pUserData;
    if (iType!=2 || pRef->pSocket==pSocket) return 0;
    if (SocketInfo(pSocket,0x70726f74,0,0,0)==254) { pData+=2; iLength-=2; }
    const u8* pAddress=(const u8*)pTo;
    u32 port=((u32)pAddress[2]<<8)|pAddress[3];
    u32 addr=((u32)pAddress[4]<<24)|((u32)pAddress[5]<<16)|((u32)pAddress[6]<<8)|pAddress[7];
    NetCritEnter(&pRef->TunnelsCritS);
    s32 result=0;
    bool found=false;
    for (s32 i=0;i<pRef->iNumTunnels && !found;++i) {
        ProtoTunnelT* pTunnel=&pRef->Tunnels[i];
        if (pTunnel->uVirtualAddr!=addr) continue;
        for (s32 j=0;j<8;++j) if (pTunnel->Info.aRemotePortList[j]==port) {
            found=true;
            result=ProtoTunnelBufferPacket(pRef,pTunnel,pData,iLength,j);
            break;
        }
    }
    if (!found)
        NetPrintfVerboseCode(pRef->iVerbosity,1,"prototunnel: [send] no match for data sent to %s:%d\n",SocketInAddrGetText(addr),port);
    NetCritLeave(&pRef->TunnelsCritS);
    return result;
}
static s32 ProtoTunnelSocketInit(ProtoTunnelRefT* pRef, s32 iPort)
{
    pRef->pSocket=SocketOpen(2,2,254);
    if (!pRef->pSocket) { NetPrintf(("prototunnel: unable to open socket\n")); return -1; }
    sockaddr addr;
    memset(&addr,0,sizeof(addr));
    addr.sa_family=2;
    addr.sa_data[0]=(char)(iPort>>8); addr.sa_data[1]=(char)iPort;
    s32 result=SocketBind(pRef->pSocket,&addr,16);
    if (result != 0) {
        NetPrintf(("prototunnel: error %d binding to port %d, trying random\n",result,iPort));
        addr.sa_data[0]=addr.sa_data[1]=0;
        result=SocketBind(pRef->pSocket,&addr,16);
        if (result != 0) {
            NetPrintf(("prototunnel: error %d binding to port\n",result));
            SocketClose(pRef->pSocket); pRef->pSocket=0; return -1;
        }
    }
    SocketInfo(pRef->pSocket,0x62696e64,0,&addr,16);
    u32 port=((u8)addr.sa_data[1]<<8)|(u8)addr.sa_data[0];
    NetPrintf(("prototunnel: bound socket to port %d\n",port));
    pRef->uTunnelPort=port;
    SocketCallback(pRef->pSocket,2,100,pRef,ProtoTunnelRecvCallback);
    return 0;
}
extern "C" ProtoTunnelRefT* ProtoTunnelCreate(s32 iMaxTunnels,s32 iTunnelPort)
{
    s32 group=DirtyMemGroupQuery();
    s32 size=128+2940*iMaxTunnels;
    ProtoTunnelRefT* pRef=(ProtoTunnelRefT*)DirtyMemAlloc(size,0x7074756e,group);
    if (!pRef) { NetPrintf(("prototunnel: could not allocate module state\n")); return 0; }
    memset(pRef,0,size);
    pRef->iMemGroup=group; pRef->iMaxTunnels=(s8)iMaxTunnels;
    pRef->uTunnelPort=iTunnelPort; pRef->uVirtualAddr=0x01000000; pRef->eMode=2;
    if (ProtoTunnelSocketInit(pRef,iTunnelPort)<0) { DirtyMemFree(pRef,0x7074756e,group); return 0; }
    NetCritInit(&pRef->TunnelsCritS,"prototunnel-global-send");
    NetCritInit(&pRef->TunnelsCritR,"prototunnel-global-recv");
    SocketControl(0,0x73646362,0,reinterpret_cast<void*>(ProtoTunnelSendCallback),pRef);
    pRef->uLastFlush=NetTick();
    return pRef;
}
extern "C" void ProtoTunnelDestroy(ProtoTunnelRefT* pRef)
{
    SocketControl(0,0x73646362,0,0,0);
    SocketClose(pRef->pSocket);
    NetCritKill(&pRef->TunnelsCritR); NetCritKill(&pRef->TunnelsCritS);
    DirtyMemFree(pRef,0x7074756e,pRef->iMemGroup);
}
extern "C" s32 ProtoTunnelAlloc(ProtoTunnelRefT* pRef,ProtoTunnelInfoT* pInfo,const char* pKey)
{
    NetCritEnter(&pRef->TunnelsCritS); NetCritEnter(&pRef->TunnelsCritR);
    for (s32 i=0;i<pRef->iNumTunnels;++i) {
        ProtoTunnelT* pTunnel=&pRef->Tunnels[i];
        if (pTunnel->Info.uRemoteClientId!=pInfo->uRemoteClientId) continue;
        NetPrintf(("prototunnel: refcounting tunnel with id=0x%08x key=%s clientId=0x%08x\n",pTunnel->uVirtualAddr,pKey,pTunnel->Info.uRemoteClientId));
        s32 result=(s32)pTunnel->uVirtualAddr, freeKey=-1;
        ++pTunnel->uRefCount;
        for (s32 j=0;j<8;++j) {
            if (!strcmp(pTunnel->aKeyList[j],pKey)) NetPrintf(("prototunnel: warning - duplicate key %s on alloc\n",pKey));
            if (freeKey==-1 && !pTunnel->aKeyList[j][0]) freeKey=j;
        }
        if (freeKey==-1) { NetPrintf(("prototunnel: error - key overflow on alloc\n")); result=-1; }
        else ds_strnzcpy(pTunnel->aKeyList[freeKey],pKey,128);
        NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS); return result;
    }
    if (pRef->iNumTunnels==pRef->iMaxTunnels) {
        NetPrintf(("prototunnel: could not allocate a new tunnel\n"));
        NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS); return -1;
    }
    ProtoTunnelT* pTunnel=&pRef->Tunnels[pRef->iNumTunnels++];
    memset(pTunnel,0,sizeof(*pTunnel)); memcpy(&pTunnel->Info,pInfo,sizeof(*pInfo));
    NetCritInit(&pTunnel->PacketCrit,"prototunnel-tunnel");
    CryptArc4Init(&pTunnel->CryptSendState,(const u8*)pKey,(s32)strlen(pKey),1);
    ds_strnzcpy(pTunnel->aKeyList[0],pKey,128);
    pTunnel->uRefCount=1; pTunnel->Info.aPortFlags[7]=1; pTunnel->bSendCtrlInfo=1;
    pTunnel->uVirtualAddr=pRef->uVirtualAddr++;
    if (!pTunnel->Info.uRemotePort) pTunnel->Info.uRemotePort=(u16)pRef->uTunnelPort;
    pTunnel->uLastStatUpdate=NetTick();
    NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS);
    NetPrintf(("prototunnel: creating map to remote client %s:%d (id=0x%08x)\n",SocketInAddrGetText(pTunnel->Info.uRemoteAddr),pTunnel->Info.uRemotePort,pTunnel->Info.uRemoteClientId));
    for (s32 i=0;i<8 && pTunnel->Info.aRemotePortList[i];++i) NetPrintf(("prototunnel: [%d] %d\n",i,pTunnel->Info.aRemotePortList[i]));
    NetPrintf(("prototunnel: allocated tunnel with id 0x%08x key=%s\n",pTunnel->uVirtualAddr,pTunnel->aKeyList[0]));
    return (s32)pTunnel->uVirtualAddr;
}
extern "C" void ProtoTunnelFree(ProtoTunnelRefT* pRef,u32 id,const char* key)
{
    NetCritEnter(&pRef->TunnelsCritS); NetCritEnter(&pRef->TunnelsCritR);
    bool found=false;
    for (s32 i=0;i<pRef->iNumTunnels;++i) {
        ProtoTunnelT* pTunnel=&pRef->Tunnels[i];
        if (pTunnel->uVirtualAddr!=id) continue;
        found=true;
        if (pTunnel->uRefCount==1) {
            NetPrintf(("prototunnel: freeing tunnel 0x%08x\n",id));
            ProtoTunnelBufferSend(pRef,pTunnel); NetCritKill(&pTunnel->PacketCrit);
            if (i!=pRef->iNumTunnels-1) memmove(pTunnel,pTunnel+1,(pRef->iNumTunnels-i-1)*sizeof(*pTunnel));
            --pRef->iNumTunnels;
        } else {
            NetPrintf(("prototunnel: decrementing refcount of tunnel 0x%08x (clientId=0x%08x)\n",id,pTunnel->Info.uRemoteClientId));
            s32 j=0;
            for (;j<8;++j) if (!strcmp(key,pTunnel->aKeyList[j])) { memset(pTunnel->aKeyList[j],0,128); break; }
            if (pTunnel->uSendKey==j) {
                NetPrintf(("prototunnel: free of current send key; picking another\n"));
                for (s32 k=0;k<8;++k) if (pTunnel->aKeyList[k][0]) {
                    NetPrintf(("prototunnel: picking key %d (%s) offset=%d\n",k,pTunnel->aKeyList[k],pTunnel->uSendOffset));
                    CryptArc4Init(&pTunnel->CryptSendState,(const u8*)pTunnel->aKeyList[k],(s32)strlen(pTunnel->aKeyList[k]),1);
                    CryptArc4Advance(&pTunnel->CryptSendState,pTunnel->uSendOffset*4);
                    pTunnel->uSendKey=(u8)k; break;
                }
            } else if (j==8) NetPrintf(("prototunnel: could not find key %s in key list on free\n",key));
            --pTunnel->uRefCount;
        }
        break;
    }
    if (pRef->iNumTunnels>0 && !found) NetPrintf(("prototunnel: could not find tunnel with id 0x%08x to free\n",id));
    NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS);
}
extern "C" s32 ProtoTunnelControl(ProtoTunnelRefT* pRef,s32 select,s32 value,s32,const void*)
{
    switch (select) {
    case 0x636c6964: NetPrintf(("prototunnel: setting local clientId=0x%08x\n",value)); pRef->uLocalClientId=value; return 0;
    case 0x64726f70: pRef->iPacketDrop=value; return 0;
    case 0x666c7368:
        NetCritEnter(&pRef->TunnelsCritS); NetCritEnter(&pRef->TunnelsCritR);
        for (s32 i=0;i<pRef->iNumTunnels;++i) if (pRef->Tunnels[i].uVirtualAddr==(u32)value) {
            NetPrintf(("prototunnel: explicitly flushing tunnel 0x%08x\n",pRef->Tunnels[i].uVirtualAddr));
            ProtoTunnelBufferSend(pRef,&pRef->Tunnels[i]); break;
        }
        NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS); return 0;
    case 0x6d6f6465: pRef->eMode=value; return 0;
    case 0x72617465: pRef->uFlushRate=value; return 0;
    case 0x7370616d: pRef->iVerbosity=(s8)value; return 0;
    default: return SocketControl(0,select,value,0,0);
    }
}
extern "C" void ProtoTunnelUpdate(ProtoTunnelRefT* pRef)
{
    u32 tick=NetTick();
    if ((s32)(tick-pRef->uLastFlush)>=(s32)pRef->uFlushRate) {
        NetCritEnter(&pRef->TunnelsCritS); NetCritEnter(&pRef->TunnelsCritR);
        for (s32 i=0;i<pRef->iNumTunnels;++i) ProtoTunnelBufferSend(pRef,&pRef->Tunnels[i]);
        NetCritLeave(&pRef->TunnelsCritR); NetCritLeave(&pRef->TunnelsCritS);
        pRef->uLastFlush=tick;
    }
}
