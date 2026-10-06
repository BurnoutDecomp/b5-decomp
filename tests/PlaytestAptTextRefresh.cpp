#include <cstdio>
#include <cstdint>
static unsigned gChecks,gFailures;
static bool gCaptured;
using AptAssetString=void*;
static intptr_t gAptEmptyTextRenderDataZID=-2;
struct {AptAssetString pCurrString;} params;
struct AptRenderItemDynamicText {
    uint32_t mStateFlags=0;
    intptr_t mZID=0;
    unsigned releases=0;
    void SetZID(intptr_t value) {
        if(mZID!=value&&mZID!=0&&mZID!=gAptEmptyTextRenderDataZID) ++releases;
        mZID=value;
    }
};
struct AptCharacterTextInst {
    AptRenderItemDynamicText* mpItem;
    AptRenderItemDynamicText* mpClone;
    bool mbClone;
    unsigned writes=0;
    AptRenderItemDynamicText* GetRenderItem() {return mpItem;}
    AptRenderItemDynamicText* GetRenderItemWritable() {
        ++writes;
        if(mbClone) {mbClone=false;mpItem=mpClone;}
        return mpItem;
    }
};
struct AptCIH {
    AptCharacterTextInst* mpInst;
    AptCharacterTextInst* GetCharacterInst() {return mpInst;}
    void Capture();
};
#include "playtest_apt_text_refresh_bodies.inc"
static void Check(bool b,const char* label) {++gChecks;if(!b){++gFailures;std::printf("FAIL %s\n",label);}}
int main() {
    // Native140836A79 reloads [charInst+8] after SetZID140836A70. A
    // writable clone has ZID0 while the previous rendered item retains its
    // live handle. Using that old handle can retire another frame's string.
    const intptr_t ids[3]={0,-2,17};
    for(int valid=0;valid<2;++valid) for(int clone=0;clone<2;++clone) for(int id=0;id<3;++id) {
        AptRenderItemDynamicText old, writable;
        old.mZID=ids[id];old.mStateFlags=valid;
        writable.mStateFlags=valid;writable.mZID=0;
        AptCharacterTextInst inst{&old,&writable,clone!=0};AptCIH cih{&inst};
        gCaptured=false;params.pCurrString=reinterpret_cast<void*>(99);cih.Capture();
        Check(gCaptured==(valid==0),"valid text does not rebuild");
        if(!valid) {
            const intptr_t expected=(ids[id]==gAptEmptyTextRenderDataZID)?0:inst.mpItem->mZID;
            Check(reinterpret_cast<intptr_t>(params.pCurrString)==expected,"allocator sees the current writable item's handle");
            Check(clone&&ids[id]==17?old.mZID==17&&old.releases==0:old.mZID==(ids[id]==17?0:ids[id]),"previous rendered item's live handle survives a writable clone");
        } else {
            Check(inst.writes==0,"valid text never requests a writable clone");
            Check(old.mZID==ids[id],"valid text retains its existing handle");
        }
    }
    std::printf("PlaytestAptTextRefresh: %u checks, %u failures\n",gChecks,gFailures);return gFailures?1:0;
}
