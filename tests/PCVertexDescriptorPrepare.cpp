#include <windows.h>
#include <d3d9.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "types.hpp"

namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }
namespace renderengine {
static IDirect3DDevice9* device=nullptr;
static IDirect3DDevice9* Dev(){return device;}
static void LogOnce(const char*,const char*){}
}
#include "pc_vertex_descriptor_prepare.inc"
static int checks,failures;
static void Check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL %s\n",text);}}
template<class T> static void Store(u8* p,T value){std::memcpy(p,&value,sizeof(value));}
static void MakeDescriptor(u8* p){
    std::memset(p,0,64);Store<u16>(p+8,2);
    Store<u32>(p+16+4,0x2A23B9);p[16+9]=D3DDECLUSAGE_POSITION;
    Store<u16>(p+32+2,12);Store<u32>(p+32+4,0x2C23A5);p[32+9]=D3DDECLUSAGE_TEXCOORD;
    p[48]=20;
}
static std::vector<u8> Snapshot(){
    using namespace renderengine;std::vector<u8> bytes;
    const auto add=[&](const auto& value){auto p=reinterpret_cast<const u8*>(&value);bytes.insert(bytes.end(),p,p+sizeof(value));};
    add(sbLastDeclHasTexcoord0);add(suLastDeclUsageMask);add(suLastDeclSourceStride);add(suLastDeclDec3nCount);
    add(sau16LastDeclDec3nOffsets);add(su8LastDeclPositionType);add(suLastDeclPositionSourceOffset);
    add(suLastDeclPositionExpandedOffset);add(spLastDeclElements);add(sau16LastDeclTexcoordOffset);
    add(sau8LastDeclTexcoordType);add(sau16LastDeclBlendOffset);add(sau8LastDeclBlendType);return bytes;
}
int main(){
    using namespace renderengine;
    alignas(16) u8 first[64],second[64],repair[64];MakeDescriptor(first);MakeDescriptor(second);MakeDescriptor(repair);
    Check(!WorldVd32_PrepareResource(first)&&sVdCache.empty(),"device-less fixup leaves no failed cache entry");
    HWND window=CreateWindowA("STATIC","Vertex descriptor checks",WS_OVERLAPPEDWINDOW,0,0,64,64,
                              nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window;pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    if(!d3d||FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device)))return 2;
    sbLastDeclHasTexcoord0=true;suLastDeclUsageMask=0x1234567812345678ull;suLastDeclSourceStride=77;
    suLastDeclDec3nCount=3;sau16LastDeclDec3nOffsets[0]=19;
    const auto before=Snapshot();u8 inputBefore[64];std::memcpy(inputBefore,first,64);
    Check(WorldVd32_PrepareResource(first)&&sVdCache.size()==1,"fixup creates a native cached declaration");
    Check(Snapshot()==before,"preparation preserves every published layout field");
    Check(std::memcmp(first,inputBefore,64)==0,"native COM pointers are not written into serialized descriptor bytes");
    auto* cached=sVdCache.begin()->second.mpDeclaration;
    D3DVERTEXELEMENT9 elements[17]{};UINT count=17;
    Check(SUCCEEDED(cached->GetDeclaration(elements,&count))&&count==3
        &&elements[0].Type==D3DDECLTYPE_FLOAT3&&elements[0].Usage==D3DDECLUSAGE_POSITION
        &&elements[1].Type==D3DDECLTYPE_FLOAT2&&elements[1].Offset==12&&elements[1].Usage==D3DDECLUSAGE_TEXCOORD,
        "created declaration retains the expected position and texture-coordinate formats");
    u32 stride=0;
    Check(WorldVd32_GetDeclaration(first,&stride)==cached&&stride==20&&sVdCache.size()==1,
        "first draw reuses the declaration prepared at fixup");
    Check(sbLastDeclHasTexcoord0&&suLastDeclSourceStride==20&&suLastDeclUsageMask==(DeclUsageBit(0,0)|DeclUsageBit(5,0)),
        "explicit draw lookup still publishes its own layout");
    const auto published=Snapshot();
    Check(WorldVd32_PrepareResource(second)&&Snapshot()==published,
        "preparing another resource preserves the active descriptor's layout and text pointer");
    Store<u32>(repair+20,0xDEADBEEF);
    Check(!WorldVd32_PrepareResource(repair)&&sVdCache.size()==2,"failed preparation does not poison the cache");
    MakeDescriptor(repair);
    Check(WorldVd32_PrepareResource(repair)&&sVdCache.size()==3,"a later valid preparation retries the same address");
    WorldVd32_OnResourceMemoryFreed(first,sizeof(first));
    Check(sVdCache.size()==2&&sVdCache.count(reinterpret_cast<uintptr_t>(first))==0,
        "resource retirement removes only its own prepared declaration");
    WorldVd32_ReleaseAll();Check(sVdCache.empty(),"device teardown releases every prepared declaration");
    device->Release();device=nullptr;d3d->Release();DestroyWindow(window);
    std::printf("PCVertexDescriptorPrepare: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
