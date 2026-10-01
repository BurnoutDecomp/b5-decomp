#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include "pc/gcm/renderengine/WorldGeometryPCLeaf.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include "pc/gcm/renderengine/PackedNormalPCLeaf.h"
namespace renderengine {
#include "pc_packed_normal.inc"
}
using namespace renderengine;
static unsigned suChecks=0,suFailures=0;
static volatile unsigned suWitness=0;
static void Check(bool lbPass,const char* lpcLabel){++suChecks;if(!lbPass){++suFailures;std::printf("FAIL %s\n",lpcLabel);}}
static std::vector<u8> Oracle(const WorldGeometryVertexPlan& lrPlan){
    std::vector<u8> lOut;
    const u8* lpBytes=static_cast<const u8*>(lrPlan.mpData);
    for(unsigned luV=0;luV<lrPlan.muNumVertices;++luV){
        const u8* lpVertex=lpBytes+luV*lrPlan.muSourceStride;
        unsigned luPosition=0;
        for(unsigned luN=0;luN<lrPlan.muDec3nCount;++luN){
            unsigned luOffset=lrPlan.mau16Dec3nOffsets[luN];
            lOut.insert(lOut.end(),lpVertex+luPosition,lpVertex+luOffset);
            u32 luPacked;std::memcpy(&luPacked,lpVertex+luOffset,4);
            for(unsigned luC=0;luC<3;++luC){
                const int liSigned=int(((luPacked>>(10*luC))&1023u)^512u)-512;
                const float lfValue=(std::max)(-1.f,static_cast<float>(liSigned)/511.f);
                const u8* lpValue=reinterpret_cast<const u8*>(&lfValue);
                lOut.insert(lOut.end(),lpValue,lpValue+4);
            }
            luPosition=luOffset+4;
        }
        lOut.insert(lOut.end(),lpVertex+luPosition,lpVertex+lrPlan.muSourceStride);
    }
    return lOut;
}
static bool Exact(const WorldGeometryVertexPlan& lrPlan){
    const auto lExpected=Oracle(lrPlan);
    std::vector<u8> lOut(lExpected.size()+32,0xcd);
    BakeVertexData(lOut.data()+16,lrPlan);
    return std::equal(lExpected.begin(),lExpected.end(),lOut.begin()+16)
        && std::all_of(lOut.begin(),lOut.begin()+16,[](u8 c){return c==0xcd;})
        && std::all_of(lOut.end()-16,lOut.end(),[](u8 c){return c==0xcd;});
}
static double Clock(){LARGE_INTEGER c,f;QueryPerformanceCounter(&c);QueryPerformanceFrequency(&f);return double(c.QuadPart)/f.QuadPart;}
int main(){
    _putenv_s("BRN_FRAME_PROFILE","0");
    struct Layout{unsigned stride;std::vector<u16> offsets;};
    const Layout laLayouts[]={{16,{12}},{24,{12,16}},{32,{12,16}},{36,{12,20,28}},{13,{1,5,9}},{20,{0}},{48,{4,12,20,28}},{16,{}}};
    for(const auto& lrLayout:laLayouts){
        WorldGeometryVertexPlan lPlan{};lPlan.muSourceStride=lrLayout.stride;
        lPlan.muExpandedStride=lrLayout.stride+unsigned(lrLayout.offsets.size())*8;
        lPlan.muDec3nCount=unsigned(lrLayout.offsets.size());
        for(unsigned lu=0;lu<lPlan.muDec3nCount;++lu)lPlan.mau16Dec3nOffsets[lu]=lrLayout.offsets[lu];
        lPlan.muNumVertices=1024;
        std::vector<u8> lInput(lPlan.muSourceStride*lPlan.muNumVertices);
        for(unsigned luV=0;luV<lPlan.muNumVertices;++luV){
            for(unsigned lu=0;lu<lPlan.muSourceStride;++lu)lInput[luV*lPlan.muSourceStride+lu]=u8(luV*31+lu*13);
            const u32 luPacked=luV|(((luV*37)%1024)<<10)|(((luV*971)%1024)<<20)|((luV%4)<<30);
            for(auto luOffset:lrLayout.offsets)std::memcpy(lInput.data()+luV*lPlan.muSourceStride+luOffset,&luPacked,4);
        }
        lPlan.mpData=lInput.data();Check(Exact(lPlan),"all1024values in every channel plus unused high bits and record bytes");
        unsigned luSeed=0x952123bu;
        for(auto& lrByte:lInput){luSeed^=luSeed<<13;luSeed^=luSeed>>17;luSeed^=luSeed<<5;lrByte=u8(luSeed);}
        Check(Exact(lPlan),"random components,unaligned records,prefixes/tails and guard bytes");
        for(unsigned luCount:{0u,1u,3u,511u,1023u}){
            lPlan.muNumVertices=luCount;Check(Exact(lPlan),"partial buffers retain exact write extent");
        }
        if(std::getenv("BRN_NORMAL_BENCH")){
            lPlan.muNumVertices=65536;lInput.resize(lPlan.muSourceStride*lPlan.muNumVertices);
            for(auto& lrByte:lInput){luSeed^=luSeed<<13;luSeed^=luSeed>>17;luSeed^=luSeed<<5;lrByte=u8(luSeed);}
            lPlan.mpData=lInput.data();std::vector<u8> lOut(lPlan.muExpandedStride*lPlan.muNumVertices);
            double lfStart=Clock();
            for(unsigned lu=0;lu<100;++lu){BakeVertexData(lOut.data(),lPlan);suWitness^=lOut[lu%lPlan.muExpandedStride];}
            std::printf("normal_bench lookup=%d stride=%u normals=%u ms=%.6f\n",PackedNormalPC::Enabled(),lPlan.muSourceStride,lPlan.muDec3nCount,(Clock()-lfStart)*10.);
        }
    }
    SYSTEM_INFO lInfo{};GetSystemInfo(&lInfo);
    auto* lpInput=static_cast<u8*>(VirtualAlloc(nullptr,lInfo.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    auto* lpOutput=static_cast<u8*>(VirtualAlloc(nullptr,lInfo.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    DWORD luOld;
    if(!lpInput||!lpOutput||!VirtualProtect(lpInput+lInfo.dwPageSize,lInfo.dwPageSize,PAGE_NOACCESS,&luOld)
       ||!VirtualProtect(lpOutput+lInfo.dwPageSize,lInfo.dwPageSize,PAGE_NOACCESS,&luOld))return 2;
    WorldGeometryVertexPlan lEdge{};lEdge.mpData=lpInput+lInfo.dwPageSize-13;
    lEdge.muNumVertices=1;lEdge.muSourceStride=13;lEdge.muExpandedStride=37;lEdge.muDec3nCount=3;
    lEdge.mau16Dec3nOffsets[0]=1;lEdge.mau16Dec3nOffsets[1]=5;lEdge.mau16Dec3nOffsets[2]=9;
    std::memset(const_cast<void*>(lEdge.mpData),0xff,13);
    const auto lExpected=Oracle(lEdge);
    u8* lpEdgeOutput=lpOutput+lInfo.dwPageSize-37;BakeVertexData(lpEdgeOutput,lEdge);
    Check(std::memcmp(lpEdgeOutput,lExpected.data(),37)==0,"last unaligned record touches neither guard page");
    VirtualFree(lpInput,0,MEM_RELEASE);VirtualFree(lpOutput,0,MEM_RELEASE);
    std::printf("PCPackedNormal: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
