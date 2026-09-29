#include "pc/gcm/renderengine/WorldGeometryPCLeaf.cpp"

namespace renderengine {
    IDirect3DDevice9* gDevice = nullptr;
    void WorldVd32_OnResourceMemoryFreed(const void*,size_t) {}
    void WorldVd32_ReleaseAll() {}
}
namespace BrnDiag { void LogBoundSurfaces(const char*,u32,bool) {} }
namespace CgsDev { namespace Log { void WriteToLog(const char*) {} } }

static int checks, failures;
static void Check(bool pass,const char* name) {
    ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n",name); }
}
static DWORD DrawPixel(const renderengine::WorldGeometryDraw& draw,unsigned base=0) {
    auto device=renderengine::gDevice;
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
    device->BeginScene();
    Check(SUCCEEDED(renderengine::WorldGeometry_Submit(draw,base)),"native retained draw succeeds");
    device->EndScene();
    IDirect3DSurface9* back=nullptr; IDirect3DSurface9* readback=nullptr;
    DWORD pixel=0;
    if (SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) &&
        SUCCEEDED(device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)) &&
        SUCCEEDED(device->GetRenderTargetData(back,readback))) {
        D3DLOCKED_RECT lock={};
        if (SUCCEEDED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY))) {
            std::memcpy(&pixel,static_cast<const char*>(lock.pBits)+16*lock.Pitch+16*4,4);
            readback->UnlockRect();
        }
    }
    if (readback) readback->Release(); if (back) back->Release();
    return pixel&0x00ffffff;
}
int main() {
    using namespace renderengine;
    HWND window=CreateWindowA("STATIC","Geometry buffers",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth=pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.hDeviceWindow=window; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    if (!d3d || FAILED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&gDevice))) return 2;
    gDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
    gDevice->SetRenderState(D3DRS_LIGHTING,FALSE); gDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    struct Vertex { float x,y,z,w; DWORD colour; };
    Vertex vertices[]={{0,0,0.5f,1,0xff00ff00},{64,0,0.5f,1,0xff00ff00},{0,64,0.5f,1,0xff00ff00}};
    const unsigned short indices[]={0,1,2};
    WorldGeometryVertexPlan vp={}; vp.mpHeader=vp.mpData=vertices; vp.muNumVertices=3;
    vp.muSourceStride=vp.muExpandedStride=sizeof(Vertex);
    WorldGeometryIndexPlan ip={}; ip.mpHeader=ip.mpRun=indices; ip.muIndexCount=3;
    ip.miMappedPrimitiveType=D3DPT_TRIANGLELIST; ip.muMappedPrimitiveCount=1;
    WorldGeometryDraw first={},second={};
    Check(WorldGeometry_Prepare(vp,ip,&first)==E_WORLDGEOMETRY_READY,"production geometry creates GPU buffers");
    if (first.mpVertexBuffer && first.mpIndexBuffer) {
        Check(first.muMinIndex==0 && first.muMaxIndex==2,"16-bit triangle list retains its exact vertex range");
        D3DVERTEXBUFFER_DESC vd={}; D3DINDEXBUFFER_DESC id={};
        static_cast<IDirect3DVertexBuffer9*>(first.mpVertexBuffer)->GetDesc(&vd);
        static_cast<IDirect3DIndexBuffer9*>(first.mpIndexBuffer)->GetDesc(&id);
        Check(vd.Pool==D3DPOOL_DEFAULT && id.Pool==D3DPOOL_DEFAULT,"immutable streams use the native GPU pool");
        Check((vd.Usage&D3DUSAGE_WRITEONLY) && (id.Usage&D3DUSAGE_WRITEONLY),"both streams remain write-only");
        Check(DrawPixel(first)==0x00ff00,"uploaded vertex and index data produce the expected pixel");
        Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY && first.mpVertexBuffer==second.mpVertexBuffer && first.mpIndexBuffer==second.mpIndexBuffer,
              "repeat draw reuses the same native allocations");
        WorldGeometry_OnResourceMemoryFreed(vertices,sizeof(vertices));
        for (auto& vertex:vertices) vertex.colour=0xffff0000;
        Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY,"reused source address is rebuilt after streaming retirement");
        Check(DrawPixel(second)==0xff0000,"retired GPU geometry cannot survive source address reuse");
    }
    Vertex offsetVertices[7]={};
    for (unsigned i=0;i<3;++i) offsetVertices[i+4]=vertices[i];
    const unsigned int strips[]={3,4,5,0xffffffffu,3,4,5};
    vp.mpHeader=vp.mpData=offsetVertices; vp.muNumVertices=7;
    ip.mpHeader=ip.mpRun=strips; ip.muIndexCount=7; ip.mb32Bit=true;
    ip.mbResetEnabled=true; ip.muResetIndex=0xffffffffu;
    ip.miMappedPrimitiveType=D3DPT_TRIANGLESTRIP; ip.muMappedPrimitiveCount=5;
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY,"32-bit reset strips produce retained buffers");
    if (second.mpVertexBuffer && second.mpIndexBuffer) {
        Check(second.muMinIndex==3 && second.muMaxIndex==5 && second.muPrimitiveCount==2,
              "range comes from final triangles and excludes reset markers");
        Check(DrawPixel(second,1)==0xff0000,"nonzero minimum index and base vertex preserve rendered geometry");
    }
    const unsigned int triangleTail[]={0,1,2,0xffffffffu};
    vp.mpHeader=vp.mpData=vertices; vp.muNumVertices=3;
    ip.mpHeader=ip.mpRun=triangleTail; ip.muIndexCount=4; ip.mb32Bit=true; ip.mbResetEnabled=false;
    ip.miMappedPrimitiveType=D3DPT_TRIANGLELIST; ip.muMappedPrimitiveCount=1;
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY,"incomplete triangle-list tail is accepted");
    Check(second.muMinIndex==0 && second.muMaxIndex==2,"unused 32-bit tail cannot wrap or inflate the vertex range");
    Check(DrawPixel(second)==0xff0000,"valid triangle before unused sentinel still renders");
    const unsigned short lineTail[]={0,1,0xffffu};
    ip.mpHeader=ip.mpRun=lineTail; ip.muIndexCount=3; ip.mb32Bit=false;
    ip.miMappedPrimitiveType=D3DPT_LINELIST; ip.muMappedPrimitiveCount=1;
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY && second.muMinIndex==0 && second.muMaxIndex==1,
          "unused 16-bit line-list tail cannot inflate the vertex range");
    WorldGeometry_ReleaseAll();
    gDevice->Release(); gDevice=nullptr; d3d->Release(); DestroyWindow(window);
    std::printf("PCWorldGeometryBuffers: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
