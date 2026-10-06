#include "SDKs/EATech/include/Apt/AptPseudoData.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <initializer_list>

struct AptCharacter {};
struct AptUint32CXForm {};
class EAStringC {
public:
    const char* mpText = nullptr;
    EAStringC() = default;
    explicit EAStringC(const char* p) : mpText(p) {}
};
struct AptCIH {
    int miCreated = -1;
    void SetCreatedOnFrame(int v) { miCreated = v &0x3fff; }
};
struct AptMergeSourceNode { void* mpRecord; AptPseudoData_t* mpProps; int32_t mnDepth; AptMergeSourceNode* mpNext; };
struct AptPseudoCIH_t { AptPseudoData_t* mpPseudoData; };
static const void* gpClip;
static const char* gpName;
static int giDepth,giClipDepth,giForce;
static double gfRatio;
static AptCharacter* gpCharacter;
static AptCIH gPlaced;
struct AptDisplayList {
    AptCIH* placeObjectNCXForm(AptCIH*,int depth,AptCharacter* pCharacter,
        const EAStringC* pName,AptCIH*,int force,int clipDepth,double ratio,
        const float*,const void* clip,const AptUint32CXForm*) {
        gpClip=clip;gpName=pName!=nullptr?pName->mpText:nullptr;giDepth=depth;
        giClipDepth=clipDepth;giForce=force;gfRatio=ratio;gpCharacter=pCharacter;
        return &gPlaced;
    }
};
static int32_t CmdI32(const void* p,size_t n) { int32_t v;std::memcpy(&v,static_cast<const char*>(p)+n,4);return v; }
static float CmdF32(const void* p,size_t n) { float v;std::memcpy(&v,static_cast<const char*>(p)+n,4);return v; }
#include "apt_snapshot_seek_bodies.inc"
static unsigned guChecks,guFailures;
static void Check(bool b,const char* msg) { ++guChecks;if(!b){++guFailures;std::printf("FAIL %s\n",msg);} }
static const void* Clip(const AptPseudoData_t& v) { const void* p;std::memcpy(&p,reinterpret_cast<const char*>(&v)+0x18,sizeof(p));return p; }
int main() {
    Check(sizeof(AptPseudoData_t)==48,"XB1 snapshot allocation size48");
    Check(offsetof(AptPseudoData_t,mfRatio)==0x20,"XB1 ratio offset20");
    Check(offsetof(AptPseudoData_t,muxFlags)==0x24,"XB1 flags offset24");
    Check(offsetof(AptPseudoData_t,mi16CharacterId)==0x28 && offsetof(AptPseudoData_t,mi16Depth)==0x2a,"XB1 ID/clip-depth offsets28/2A");
    alignas(8) unsigned char record[128]={};
    auto* info=reinterpret_cast<AptPlaceObjectInfo_t*>(record);info->muTag=3;
    auto* body=info->GetBody();
    body->miClipDepth=0x123;body->mfRatio=0.25f;body->mpName="prompt";
    AptCharacter character;
    unsigned long long clipA=0,clipB=0;
    body->mpClipActions=&clipA;
    for(unsigned combo=0;combo<16;++combo) {
        body->muxFlags=(combo&1?4:0)|(combo&2?8:0)|(combo&4?0x10:0)|(combo&8?0x80:0);
        AptPseudoData_t data(info,0x1234,&character);
        Check(data.mpData==&character,"snapshot borrows original character");
        Check(data.mi16CharacterId==0x1234 && data.mi16Depth==0x123 && data.muxFlags==body->muxFlags,"snapshot IDs/depth/flags copied");
        Check(data.mpMatrix==(combo&1?body->maMatrix:nullptr),"snapshot matrix flag");
        Check(data.mpColorTransform==(combo&2?body->maColorTransform:nullptr),"snapshot colour flag");
        Check(Clip(data)==(combo&8?&clipA:nullptr),"snapshot preserves full native clip-action pointer under bit80");
        Check(data.mfRatio==(combo&4?0.25f:0.0f),"snapshot ratio flag");
    }
    AptDisplayList list;AptCIH parent;
    for(unsigned flags:{0u,0x20u,0x80u,0xa0u}) {
        body->muxFlags=flags|0x10;AptPseudoData_t data(info,0x1234,&character);
        AptMergeSourceNode src={info,&data,17,nullptr};
        AptFramePlacementDispatch(&list,reinterpret_cast<void**>(&src),&parent);
        Check(gpClip==(flags&0x80?&clipA:nullptr),"seek placement receives original full-width clip actions");
        Check(gpName==(flags&0x20?body->mpName:nullptr),"seek placement name flag unchanged");
        Check(giDepth==17 && giClipDepth==0x123 && giForce==1 && gfRatio==0.25 && gpCharacter==&character,"seek placement other arguments preserved");
        Check(gPlaced.miCreated==0x1234,"seek placement created-on-frame preserved");
    }
    body->muxFlags=0x90;AptPseudoData_t data(info,7,&character);
    body->mpClipActions=&clipB;body->mfRatio=0.5f;body->muxFlags=0;
    ApplyMove(data,*body);
    Check(Clip(data)==&clipA,"MOVE without bit80 retains prior handler block");
    Check(data.mfRatio==0.25f,"MOVE without bit10 retains prior ratio");
    body->muxFlags=0x80;ApplyMove(data,*body);
    Check(Clip(data)==&clipB,"MOVE with bit80 replaces full-width handler pointer");
    body->muxFlags=0x1c;ApplyMove(data,*body);
    Check(data.mpMatrix==body->maMatrix && data.mpColorTransform==body->maColorTransform,"MOVE matrix/colour captures remain original");
    Check(Clip(data)==&clipB && data.mfRatio==0.5f,"MOVE other fields preserve current clip block");
    Check(data.muxFlags==0x9c,"MOVE retains union of captured flags");
    std::printf("AptSnapshotSeek: %u checks, %u failures\n",guChecks,guFailures);
    return guFailures?1:0;
}
