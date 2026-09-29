#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

static int checks, failures;
static bool countAllocations;
static unsigned allocations;
void* operator new(size_t bytes) {
    if(countAllocations)++allocations;
    if(void* result=std::malloc(bytes?bytes:1))return result;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory,size_t) noexcept { std::free(memory); }
static void Check(bool good, const char* message) {
    ++checks; if (!good) { ++failures; std::printf("FAIL %s\n",message); }
}
#define CGS_ASSERT(good,message) do { if (!(good)) { ++failures; std::printf("ASSERT %s\n",message); } } while (0)
using namespace CgsGraphics;
alignas(128) static unsigned char arena[1024*1024];
static size_t used;
void* DispatchBin::AllocateMemoryFast(u32 words) {
    void* result=arena+used; used+=size_t(words)*16;
    if(used>sizeof(arena)) std::abort();
    return result;
}
#include "pc_dispatch_sort.inc"

// Instruction oracle for ARTIST 0x827FD4CC..0x827FD5D4 / 0x827FD768..794.
// Masks use PPC's MSB-first bit numbering; do not simplify to the production
// field expressions. The alpha-tested path deliberately masks rlwinm 12,4,15.
static u64 Mask(unsigned width,unsigned begin,unsigned end) {
    u64 result=0;for(unsigned i=begin;i<=end;++i)result|=u64(1)<<(width-1-i);return result;
}
static u32 Rol32(u32 value,unsigned bits) { return (value<<bits)|(value>>(32-bits)); }
struct Fields { u16 a,b,c,d,flags; };
static s32 Fctiwz(float value) {
    if(std::isnan(value)||value<=float(INT32_MIN))return INT32_MIN;
    if(value>=float(INT32_MAX))return INT32_MAX;
    return s32(std::trunc(value));
}
static u64 Oracle(Fields f,bool z,float depth) {
    u64 r9=f.b&Mask(64,52,63),r11=f.a&Mask(64,52,63),r10=f.d&Mask(64,61,63),r8=f.c;
    if(z) {
        r10=(f.flags>>3)&1;
        s32 converted=Fctiwz(depth*32767.0f);
        if(converted<0)converted=0;else if(converted>0x7fff)converted=0x7fff;
        r11=u64(converted)&Mask(64,49,63);
        if(r10==1) {
            r10=Rol32(u32(r9),12)&Mask(32,4,15);
            r9=r8&Mask(64,48,63);r10|=u64(0x1000)<<16;r10|=r9;r10<<=15;
        } else {
            r10=(r10&Mask(32,31,31))<<12;r9&=Mask(64,52,63);r10|=r9;
            r9=r8&Mask(64,48,63);r10<<=15;r11|=r10;r11<<=16;return r11|r9;
        }
    } else {
        r9<<=16;r10<<=32;r9|=r8;r11>>=3;r9&=Mask(64,12,63);r10|=r9;r10<<=9;
    }
    return r10|r11;
}
static u64 PreZOracle(Fields f) {
    u64 r11=(u32(f.b)&Mask(32,20,31))<<12,r10=f.c&Mask(64,52,63),r9=f.a&Mask(64,52,63);
    r11|=r10;r11<<=12;return r11|r9;
}
static MaterialTechniqueView Technique(Fields f) { return {0,0,f.flags,f.a,f.b,f.c,f.d}; }

int main() {
    std::mt19937 random(0x827fcda0);
    const float depths[]={-100.0f,-0.01f,0.0f,0.125f,0.5f,1.0f,100.0f,
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),std::nextafter(0.5f,0.0f),std::nextafter(0.5f,1.0f)};
    bool colour=true,opaque=true,alpha=true,prez=true;
    for(unsigned i=0;i<4096;++i) {
        Fields f{u16(random()),u16(random()),u16(random()),u16(random()),0};
        const float depth=depths[i%std::size(depths)];auto technique=Technique(f);
        colour&=MeshSortKey(technique,false,depth)==Oracle(f,false,depth);
        opaque&=MeshSortKey(technique,true,depth)==Oracle(f,true,depth);
        prez&=PreZSortKey(technique)==PreZOracle(f);
        f.flags=8;technique=Technique(f);alpha&=MeshSortKey(technique,true,depth)==Oracle(f,true,depth);
    }
    Check(colour,"colour shader/material/priority bits match PPC across 4096 cases");
    Check(opaque,"opaque depth keys match PPC including float conversion boundaries");
    Check(alpha,"alpha-tested depth keys preserve the original rlwinm mask");
    Check(prez,"pre-Z keys preserve all 36 bits");
    auto technique=Technique({0xabc,0xdef,0x9876,7,0});
    Check(MeshSortKey(technique,false,0)>UINT32_MAX,"colour priority survives above bit 32");
    Check(PreZSortKey(technique)>UINT32_MAX,"pre-Z pixel hash survives above bit 32");
    Check(MeshSortKey(technique,true,0.1f)<MeshSortKey(technique,true,0.9f),"equal-material depth draws sort near first");
    Fields one{0xabc,0xdef,0x9876,7,0},two=one;two.c++;
    Check(MeshSortKey(Technique(one),false,0)!=MeshSortKey(Technique(two),false,0),"colour keys retain material distinction");

    // Exercise real Submit, key-block rollover, flattening and sorting with
    // independent expected 64-bit records, including the unsigned top bit.
    std::array<DispatchCommand,4096> packets{};DispatchBin bin{};DispatchList list{};
    list.mpDispatchBin=&bin;list.m_pBinBase=packets.data();used=0;
    std::vector<u64> expected;expected.reserve(1536);
    for(unsigned i=0;i<1536;++i) {
        const u64 key=(u64(random())<<12|random()%4096)&((u64(1)<<44)-1);
        list.Submit(key,&packets[2*i]);expected.push_back((key<<20)|(2*i));
    }
    Check(list.muCount==expected.size(),"submission retains the complete packet count");
    bool records=true;size_t index=0;unsigned blocks=0;
    for(auto block=list.mpBlockListHead;block;block=block->mpNext) {
        ++blocks;for(unsigned i=0;i<block->muCount;++i) records&=block->mpKeys[i]==expected[index++];
    }
    Check(records,"Submit preserves all 44 key bits and each packet offset");
    Check(blocks==24,"key-block rollover retains all records");
    std::sort(expected.begin(),expected.end());countAllocations=true;list.SortForDispatch();countAllocations=false;
    Check(allocations==0,"sorting uses no per-frame temporary heap allocation");
    Check(list.mpSortedKeys&&std::equal(expected.begin(),expected.end(),list.mpSortedKeys),"flattened records sort in unsigned ascending order");
    Check(list.mpBlockListHead==list.mpBlockListTail&&!list.mpBlockListHead->mpNext,"flattened list replaces its old block chain");
    Check((reinterpret_cast<uintptr_t>(list.mpSortedKeys)&127)==0,"flattened key storage remains 128-byte aligned");
    list.SortForDispatch();
    Check(std::equal(expected.begin(),expected.end(),list.mpSortedKeys),"already sorted records remain unchanged");
    DispatchList empty{};empty.mpDispatchBin=&bin;empty.m_pBinBase=packets.data();empty.SortForDispatch();
    Check(empty.muCount==0&&!empty.mpSortedKeys,"empty list is safe");
    empty.Submit(0xfffffffffffULL,packets.data());empty.SortForDispatch();
    Check(empty.mpSortedKeys[0]==0xfffffffffff00000ULL,"single maximum key retains its high bits");
    DispatchList tied{};tied.mpDispatchBin=&bin;tied.m_pBinBase=packets.data();
    for(unsigned i:{9u,1u,4u,1u,8u,2u})tied.Submit(0x9abcdef0123ULL,&packets[i]);
    tied.SortForDispatch();const unsigned offsets[]={1,1,2,4,8,9};bool ties=true;
    for(unsigned i=0;i<std::size(offsets);++i)ties&=tied.mpSortedKeys[i]==((0x9abcdef0123ULL<<20)|offsets[i]);
    Check(ties,"equal material keys sort by packet offset and retain duplicate records");
    std::printf("PCDispatchSort: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
