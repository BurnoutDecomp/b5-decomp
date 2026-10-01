#include "pc/gcm/renderengine/WorldGeometryPCLeaf.cpp"
#include "pc/gcm/renderengine/AssertFramePCLeaf.h"
#include <d3dcompiler.h>

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
static void BindingProtocol() {
    using namespace renderengine::GeometryBindingsPC;
    struct Device {
        unsigned vertexCalls=0,indexCalls=0;
        bool failVertex=false,failIndex=false;
        HRESULT SetStreamSource(UINT,IDirect3DVertexBuffer9*,UINT,UINT) {
            ++vertexCalls;return failVertex?D3DERR_INVALIDCALL:S_OK;
        }
        HRESULT SetIndices(IDirect3DIndexBuffer9*) {++indexCalls;return failIndex?D3DERR_INVALIDCALL:S_OK;}
    } first,second;
    Cache cache;
    auto* vertex=reinterpret_cast<IDirect3DVertexBuffer9*>(uintptr_t(1));
    auto* index=reinterpret_cast<IDirect3DIndexBuffer9*>(uintptr_t(2));
    cache.BindVertex(&first,vertex,4,20);cache.BindIndex(&first,index);
    cache.BindVertex(&first,vertex,4,20);cache.BindIndex(&first,index);
    Check(first.vertexCalls==1&&first.indexCalls==1,"identical successful bindings avoid native calls");
    cache.BindVertex(&first,vertex,8,20);cache.BindVertex(&first,vertex,8,24);
    Check(first.vertexCalls==3,"offset and stride changes cannot reuse a stale binding");
    first.failVertex=true;
    Check(FAILED(cache.BindVertex(&first,vertex,12,24)),"failed vertex binding propagates failure");
    first.failVertex=false;cache.BindVertex(&first,vertex,12,24);
    Check(first.vertexCalls==5,"failed vertex bindings are retried even for the same request");
    cache.InvalidateUP(&first,true);first.failIndex=true;
    Check(FAILED(cache.BindIndex(&first,index)),"failed index binding propagates failure");
    first.failIndex=false;cache.BindIndex(&first,index);
    Check(first.indexCalls==3,"failed index bindings are retried even for the same request");
    cache.BindVertex(&second,vertex,12,24);cache.BindIndex(&second,index);
    Check(second.vertexCalls==1&&second.indexCalls==1,"another device cannot inherit the previous bindings");
    const auto negative=Rebase(64,20,-1),overflow=Rebase(64,4,INT_MAX),zero=Rebase(64,0,3);
    Check(negative.muOffset==64&&negative.miBase==-1&&overflow.muOffset==64&&overflow.miBase==INT_MAX&&zero.muOffset==64&&zero.miBase==3,
          "negative, overflowing and zero-stride bases retain their original addressing");
}
static DWORD ReadPixel(unsigned x=16,unsigned y=16) {
    auto device=renderengine::gDevice;
    IDirect3DSurface9* back=nullptr; IDirect3DSurface9* readback=nullptr;
    DWORD pixel=0;
    if (SUCCEEDED(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back)) &&
        SUCCEEDED(device->CreateOffscreenPlainSurface(64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)) &&
        SUCCEEDED(device->GetRenderTargetData(back,readback))) {
        D3DLOCKED_RECT lock={};
        if (SUCCEEDED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY))) {
            std::memcpy(&pixel,static_cast<const char*>(lock.pBits)+y*lock.Pitch+x*4,4);
            readback->UnlockRect();
        }
    }
    if (readback) readback->Release(); if (back) back->Release();
    return pixel&0x00ffffff;
}
static DWORD DrawPixel(const renderengine::WorldGeometryDraw& draw,unsigned base=0) {
    auto device=renderengine::gDevice;
    device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);
    device->BeginScene();
    Check(SUCCEEDED(renderengine::WorldGeometry_Submit(draw,base)),"native retained draw succeeds");
    device->EndScene();
    return ReadPixel();
}
int main() {
    _putenv_s("BRN_GEOMETRY_BIND_CACHE","1");
    BindingProtocol();
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
    // A GUI/debug UP draw clears bindings inside D3D9. The next retained draw
    // must bind them again even when its resource identities did not change.
    Vertex blue[3];std::memcpy(blue,vertices,sizeof(blue));
    for(auto& v:blue)v.colour=0xff0000ff;
    for(bool indexed:{false,true}) {
        GeometryBindingsPC::gCache.Invalidate();
        FrameProfile::Frame frame{};FrameProfile::gCapture.mpCurrent=&frame;
        gDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);gDevice->BeginScene();
        WorldGeometry_Submit(second,0);
        HRESULT up=indexed?GeometryBindingsPC::DrawIndexedPrimitiveUP(gDevice,D3DPT_TRIANGLELIST,0,3,1,indices,D3DFMT_INDEX16,blue,sizeof(Vertex))
                          :GeometryBindingsPC::DrawPrimitiveUP(gDevice,D3DPT_TRIANGLELIST,1,blue,sizeof(Vertex));
        const HRESULT restored=WorldGeometry_Submit(second,0);gDevice->EndScene();
        FrameProfile::gCapture.mpCurrent=nullptr;
        Check(SUCCEEDED(up)&&SUCCEEDED(restored)&&ReadPixel()==0xff0000,"retained geometry restores correct pixels after an UP draw");
        Check(frame.muVertexBindRequests==2&&frame.muVertexBindSkips==0&&frame.muIndexBindSkips==(indexed?0u:1u),
              "UP invalidation preserves only bindings that D3D actually retains");
    }
    // Exercise the real assert state-block restoration with a different native
    // vertex buffer bound during the overlay. Cache invalidation must follow Apply.
    IDirect3DVertexBuffer9* overlayBuffer=nullptr;void* overlayBytes=nullptr;
    gDevice->CreateVertexBuffer(sizeof(blue),D3DUSAGE_WRITEONLY,0,D3DPOOL_DEFAULT,&overlayBuffer,nullptr);
    bool overlayValid=overlayBuffer&&SUCCEEDED(overlayBuffer->Lock(0,0,&overlayBytes,0));
    if(overlayValid){std::memcpy(overlayBytes,blue,sizeof(blue));overlayBuffer->Unlock();
        WorldGeometryDraw overlay=second;overlay.mpVertexBuffer=overlayBuffer;overlay.muVertexOffset=0;
        gDevice->BeginScene();WorldGeometry_Submit(second,0);
        IDirect3DSurface9* back=nullptr;gDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
        { PCAssertFrame scope(gDevice,back);overlayValid&=scope.IsReady();
            gDevice->BeginScene();overlayValid&=SUCCEEDED(WorldGeometry_Submit(overlay,0));gDevice->EndScene(); }
        if(back)back->Release();
        gDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
        overlayValid&=SUCCEEDED(WorldGeometry_Submit(overlay,0));gDevice->EndScene();
        overlayValid&=ReadPixel()==0x0000ff;
    }
    Check(overlayValid,"assert state restoration cannot leave a stale native binding cache");
    if(overlayBuffer)overlayBuffer->Release();
    const unsigned short negativeIndices[]={1,2,3};WorldGeometryDraw negativeDraw{};
    ip.mpHeader=ip.mpRun=negativeIndices;ip.muIndexCount=3;ip.mb32Bit=false;
    Check(WorldGeometry_Prepare(vp,ip,&negativeDraw)==E_WORLDGEOMETRY_READY&&DrawPixel(negativeDraw,~0u)==0xff0000,
          "valid negative base with nonzero minimum index keeps its original pixels");
    const unsigned short lineTail[]={0,1,0xffffu};
    ip.mpHeader=ip.mpRun=lineTail; ip.muIndexCount=3; ip.mb32Bit=false;
    ip.miMappedPrimitiveType=D3DPT_LINELIST; ip.muMappedPrimitiveCount=1;
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY && second.muMinIndex==0 && second.muMaxIndex==1,
          "unused 16-bit line-list tail cannot inflate the vertex range");

    Vertex queued[]={{0,0,0.5f,1,0xff00ff00},{32,0,0.5f,1,0xff00ff00},{0,64,0.5f,1,0xff00ff00}};
    vp.mpHeader=vp.mpData=queued;vp.muNumVertices=3;
    ip.mpHeader=ip.mpRun=indices;ip.muIndexCount=3;ip.miMappedPrimitiveType=D3DPT_TRIANGLELIST;
    Check(WorldGeometry_Prepare(vp,ip,&first)==E_WORLDGEOMETRY_READY,"queued old geometry prepares");
    gDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0);gDevice->BeginScene();
    Check(SUCCEEDED(WorldGeometry_Submit(first,0)),"old geometry submits before source retirement");
    WorldGeometry_OnResourceMemoryFreed(queued,sizeof(queued));
    for(auto& v:queued){v.x+=32;v.colour=0xffff0000;}
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY,"same source address prepares new geometry while old draw is queued");
    Check(first.mpVertexBuffer!=second.mpVertexBuffer || first.muVertexOffset!=second.muVertexOffset,
          "pending GPU use keeps the retired native range unavailable");
    Check(SUCCEEDED(WorldGeometry_Submit(second,0)),"new geometry submits through its distinct range");
    gDevice->EndScene();WorldGeometry_BeginFrame();
    Check(ReadPixel(8,16)==0x00ff00 && ReadPixel(40,16)==0xff0000,"queued old and new GPU draws both retain their own pixels");
    WorldGeometry_BeginFrame();
    Check(sGeometryPool.mStatistics.muRetiredBytes==0,"completed native event releases retired ranges");

    struct PackedVertex { float x,y,z; unsigned normal; };
    const unsigned normal=511u|(512u<<10)|(513u<<20); // +1, clamped -512, -511
    PackedVertex packed[]={{-1,1,.5f,normal},{1,1,.5f,normal},{-1,-1,.5f,normal}};
    vp.mpHeader=vp.mpData=packed;vp.muSourceStride=16;vp.muExpandedStride=24;
    vp.muDec3nCount=1;vp.mau16Dec3nOffsets[0]=12;
    Check(WorldGeometry_Prepare(vp,ip,&second)==E_WORLDGEOMETRY_READY && second.muVertexOffset!=0,"DEC3N data uploads into a nonzero pooled offset");
    const char* vs="float4 main(float3 p:POSITION,float3 n:NORMAL,out float3 c:TEXCOORD0):POSITION { c=n*0.5+0.5;return float4(p,1); }";
    const char* ps="float4 main(float3 c:TEXCOORD0):COLOR { return float4(c,1); }";
    ID3DBlob* vsCode=nullptr;ID3DBlob* psCode=nullptr;
    Check(SUCCEEDED(D3DCompile(vs,std::strlen(vs),nullptr,nullptr,nullptr,"main","vs_3_0",0,0,&vsCode,nullptr)) &&
          SUCCEEDED(D3DCompile(ps,std::strlen(ps),nullptr,nullptr,nullptr,"main","ps_3_0",0,0,&psCode,nullptr)),"normal-to-colour GPU oracle compiles");
    IDirect3DVertexShader9* vertexShader=nullptr;IDirect3DPixelShader9* pixelShader=nullptr;
    IDirect3DVertexDeclaration9* declaration=nullptr;
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
                                     {0,12,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_NORMAL,0},D3DDECL_END()};
    Check(vsCode && psCode && SUCCEEDED(gDevice->CreateVertexShader(static_cast<const DWORD*>(vsCode->GetBufferPointer()),&vertexShader)) &&
          SUCCEEDED(gDevice->CreatePixelShader(static_cast<const DWORD*>(psCode->GetBufferPointer()),&pixelShader)) &&
          SUCCEEDED(gDevice->CreateVertexDeclaration(elements,&declaration)),"normal-to-colour GPU oracle creates");
    if(vertexShader&&pixelShader&&declaration){
        gDevice->SetVertexShader(vertexShader);gDevice->SetPixelShader(pixelShader);gDevice->SetVertexDeclaration(declaration);
        Check(DrawPixel(second)==0xff0000,"CPU-staged DEC3N endpoints and channel offsets survive native upload");
    }
    if(declaration)declaration->Release();if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();
    if(vsCode)vsCode->Release();if(psCode)psCode->Release();
    WorldGeometry_ReleaseAll();
    gDevice->Release(); gDevice=nullptr; d3d->Release(); DestroyWindow(window);
    std::printf("PCWorldGeometryBuffers: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
