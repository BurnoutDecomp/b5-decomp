#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "pc/gcm/renderengine/InstancingPCLeaf.h"
#include "pc/gcm/renderengine/InstancedDrawPCLeaf.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
static IDirect3DDevice9* spDevice;
static unsigned suChecks, suFailures, suFallback, suInstanced;
static renderengine::InstancingPC::Cache sgInstances;
static IDirect3DDevice9* Dev() { return spDevice; }
static void LogOnce(const char*,const char*) {}
static void LogUnsetShaderConstantOnce() {}
static void Check(bool v,const char* label) { ++suChecks; if(!v) {++suFailures;std::printf("FAIL %s\n",label);} }
namespace CgsDev { namespace Assert {
int BeginAssert(){return 0;} int FireAssert(const char*,const char*,int){++suFailures;return 0;} void* EndAssert(){return nullptr;}
} }
namespace renderengine {
void PCSetVertexShaderConstantF(IDirect3DDevice9* d,u32 r,const float* p,u32 n){d->SetVertexShaderConstantF(r,p,n);}
void PCSetPixelShaderConstantF(IDirect3DDevice9* d,u32 r,const float* p,u32 n){d->SetPixelShaderConstantF(r,p,n);}
bool WorldDraw_TryInstancedPC(u32,u32,u32,u32,const WorldInstanceDrawPC& group) {
    IDirect3DVertexShader9* vs=nullptr; IDirect3DVertexDeclaration9* vd=nullptr;
    spDevice->GetVertexShader(&vs);spDevice->GetVertexDeclaration(&vd);
    const bool began=sgInstances.Begin(spDevice,vs,vd,group.mpMatrices,group.mpIndices,group.muCount);
    if(vs)vs->Release();if(vd)vd->Release();if(!began)return false;
    ++suInstanced;
    const HRESULT hr=spDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);
    sgInstances.End();Check(SUCCEEDED(hr),"native variant executes the group");return true;
}
}
namespace shadow {
void Device::SetObjectTransformPC(const float*){} // unused fallback shader c240 boundary
void Device::DrawIndexedMeshPC(const RenderableMesh*) {
    ++suFallback;Check(SUCCEEDED(spDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1)),"actual colour VS native fallback draw");
}
}
#include "wheel_shader.inc"
static ID3DBlob* Compile(const char* source,const char* entry,const char* target) {
    ID3DBlob *code=nullptr,*errors=nullptr;const HRESULT hr=D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,entry,target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3|D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,0,&code,&errors);
    if(FAILED(hr))std::printf("compile %08X %s\n",unsigned(hr),errors?static_cast<const char*>(errors->GetBufferPointer()):"");
    if(errors)errors->Release();return code;
}
struct Input { float p[3],n[3],t[3];unsigned short uv[2]; };
struct Reference {float p[3],blur;};
static std::vector<float> Pixels(IDirect3DSurface9* target,IDirect3DSurface9* read) {
    std::vector<float> out;if(FAILED(spDevice->GetRenderTargetData(target,read)))return out;
    D3DLOCKED_RECT l={};if(FAILED(read->LockRect(&l,nullptr,D3DLOCK_READONLY)))return out;
    out.resize(128*128*4);for(unsigned y=0;y<128;++y)std::memcpy(out.data()+y*128*4,static_cast<const char*>(l.pBits)+y*l.Pitch,128*16);
    read->UnlockRect();return out;
}
int main() {
    HWND window=CreateWindowA("STATIC","installed wheel shader consumer",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);D3DPRESENT_PARAMETERS pp={};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    if(!api||FAILED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&spDevice)))return 2;
    const char* source=R"(
float4x4 modified:register(c60);
struct R {float3 p:POSITION;float blur:TEXCOORD0;};
struct O {float4 p:POSITION;float3 world:TEXCOORD0;float3 uvblur:TEXCOORD2;};
O vs(R v) {O o;o.world=v.p;float4 p=float4(v.p,1);float z=dot(p,modified[2]);
o.p=float4(dot(p,modified[0]),dot(p,modified[1]),z*modified[3].x+modified[3].y,z*modified[3].z+modified[3].w);
o.uvblur=float3(0,0,v.blur);return o;}
float4 ps(float3 world:TEXCOORD0,float3 b:TEXCOORD2):COLOR0{return float4(world,b.z);}
float4 white():COLOR0{return 1;}
)";
    auto* refCode=Compile(source,"vs","vs_3_0");auto* psCode=Compile(source,"ps","ps_3_0");auto* whiteCode=Compile(source,"white","ps_3_0");
    if(!refCode||!psCode||!whiteCode)return 2;
    IDirect3DVertexShader9* referenceVS=nullptr;IDirect3DPixelShader9 *observation=nullptr,*white=nullptr;
    spDevice->CreateVertexShader(static_cast<const DWORD*>(refCode->GetBufferPointer()),&referenceVS);
    spDevice->CreatePixelShader(static_cast<const DWORD*>(psCode->GetBufferPointer()),&observation);
    spDevice->CreatePixelShader(static_cast<const DWORD*>(whiteCode->GetBufferPointer()),&white);
    refCode->Release();psCode->Release();whiteCode->Release();if(!referenceVS||!observation||!white)return 2;
    const D3DVERTEXELEMENT9 actual[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
        {0,24,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_TANGENT,0},{0,36,D3DDECLTYPE_FLOAT16_2,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    const D3DVERTEXELEMENT9 ref[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},{0,12,D3DDECLTYPE_FLOAT1,0,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9 *actualDecl=nullptr,*refDecl=nullptr;
    spDevice->CreateVertexDeclaration(actual,&actualDecl);spDevice->CreateVertexDeclaration(ref,&refDecl);if(!actualDecl||!refDecl)return 2;
    Input vertices[3]={};for(unsigned v=0;v<3;++v) {
        std::memcpy(vertices[v].p,wheelVertices+v*24,12);std::memcpy(vertices[v].uv,wheelVertices+v*24+20,4);
        for(unsigned attr=0;attr<2;++attr) {unsigned packed;std::memcpy(&packed,wheelVertices+v*24+12+attr*4,4);
            float* out=attr?vertices[v].t:vertices[v].n;
            for(unsigned lane=0;lane<3;++lane){int c=(packed>>(lane*10))&1023;if(c&512)c-=1024;out[lane]=c==-512?-1.f:float(c)/511.f;}
        }
    }
    IDirect3DVertexBuffer9 *vb=nullptr,*refVb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;void* data=nullptr;
    spDevice->CreateVertexBuffer(sizeof(vertices),D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&vb,nullptr);
    spDevice->CreateVertexBuffer(sizeof(Reference)*12,D3DUSAGE_WRITEONLY,0,D3DPOOL_MANAGED,&refVb,nullptr);
    spDevice->CreateIndexBuffer(6,D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,nullptr);if(!vb||!refVb||!ib)return 2;
    vb->Lock(0,0,&data,0);std::memcpy(data,vertices,sizeof(vertices));vb->Unlock();unsigned short indices[3]={0,1,2};ib->Lock(0,0,&data,0);std::memcpy(data,indices,6);ib->Unlock();
    IDirect3DSurface9 *target=nullptr,*read=nullptr;
    spDevice->CreateRenderTarget(128,128,D3DFMT_A32B32G32R32F,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
    spDevice->CreateOffscreenPlainSurface(128,128,D3DFMT_A32B32G32R32F,D3DPOOL_SYSTEMMEM,&read,nullptr);if(!target||!read)return 2;
    spDevice->SetDepthStencilSurface(nullptr);spDevice->SetRenderTarget(0,target);D3DVIEWPORT9 viewport={0,0,128,128,0,1};spDevice->SetViewport(&viewport);
    spDevice->SetRenderState(D3DRS_ZENABLE,FALSE);spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    // Orthographic view of the actual radial plane; same modified-projection form as ARTIST.
    const float modified[16]={0,0,1,0, 0,1,0,0, 1,0,0,0, 0,.5f,0,1};
    spDevice->SetVertexShaderConstantF(60,modified,4);
    float matrices[5][16]={};float wheel[4][4]={};
    for(unsigned i=0;i<5;++i){float* m=matrices[i];m[0]=.24f;m[1]=.015f;m[2]=.05f;m[4]=-.035f;m[5]=.6f;m[6]=.03f;m[8]=.08f;m[9]=-.025f;m[10]=.6f;
        m[12]=float(i)*.07f;m[13]=(i&1)?.25f:-.25f;m[14]=-.75f+float(i)*.375f;m[15]=1;}
    auto* storage=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(0x15000000),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!storage||reinterpret_cast<uintptr_t>(storage)>0xffffffff)return 2;
    auto* slots=reinterpret_cast<u32*>(storage+256);slots[0]=6;slots[1]=7;
    auto* handles=storage+272;void* scratch[]={matrices,wheel};
    CgsGraphics::MaterialTechniqueView technique={};technique.muShaderTechnique=static_cast<u32>(reinterpret_cast<uintptr_t>(storage));
    auto* block=reinterpret_cast<u32*>(storage+0x1c);block[0]=2;block[1]=static_cast<u32>(reinterpret_cast<uintptr_t>(slots));block[3]=static_cast<u32>(reinterpret_cast<uintptr_t>(handles));
    RenderableMesh mesh={};mesh.mu8InstanceCount=5;mesh.mDrawIndexedParameters={4,0,0,14};
    for(unsigned zOnly=0;zOnly<2;++zOnly) {
        const unsigned char* program=zOnly?wheelZVS:wheelVS;const unsigned bytes=zOnly?sizeof(wheelZVS):sizeof(wheelVS);
        IDirect3DVertexShader9* shader=nullptr;Check(SUCCEEDED(spDevice->CreateVertexShader(reinterpret_cast<const DWORD*>(program),&shader)),"unchanged installed wheel VS loads");if(!shader)return 2;
        renderengine::InstancingPC::Program variant;
        const bool canInstance=renderengine::InstancingPC::BuildProgram(reinterpret_cast<const DWORD*>(program),bytes/4,actual,5,variant);
        Check(canInstance==bool(zOnly),"relative-shadow colour VS takes fallback; actual Z-only can instance");
        const unsigned worldReg=zOnly?4:20,viewReg=zOnly?0:12;
        handles[0]=worldReg;handles[3]=4;handles[4]=31;handles[7]=1;block[0]=zOnly?1:2;
        float constants[256][4]={};renderengine::WorldShaderConstants_Set(false,0,constants,256);
        spDevice->SetVertexShaderConstantF(viewReg,modified,4);spDevice->SetVertexShaderConstantF(60,modified,4);
        for(unsigned ordering=0;ordering<2;++ordering) {
            const unsigned perm[]={3,0,4,1};Reference expected[12]={};
            for(unsigned i=0;i<4;++i){const unsigned selected=ordering?perm[i]:i;wheel[i][0]=.125f+float(i)*.2f;wheel[i][3]=float(selected);
                for(unsigned v=0;v<3;++v){auto& r=expected[i*3+v];const float* p=vertices[v].p;const float* m=matrices[selected];
                    for(unsigned c=0;c<3;++c)r.p[c]=p[0]*m[c]+p[1]*m[4+c]+p[2]*m[8+c]+m[12+c];r.blur=wheel[i][0];}}
            refVb->Lock(0,0,&data,0);std::memcpy(data,expected,sizeof(expected));refVb->Unlock();
            spDevice->SetVertexShader(referenceVS);spDevice->SetPixelShader(zOnly?white:observation);spDevice->SetVertexDeclaration(refDecl);spDevice->SetStreamSource(0,refVb,0,sizeof(Reference));
            spDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);spDevice->BeginScene();spDevice->DrawPrimitive(D3DPT_TRIANGLELIST,0,4);spDevice->EndScene();const auto cpu=Pixels(target,read);
            spDevice->SetVertexShader(shader);spDevice->SetVertexDeclaration(actualDecl);spDevice->SetStreamSource(0,vb,0,sizeof(Input));spDevice->SetIndices(ib);
            spDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);spDevice->BeginScene();renderengine::WorldInstanceDrawPC group={matrices[0],wheel[0],modified,4};
            shadow::Device::DrawInstancedMeshPC(&mesh,&group,&technique,scratch,bool(zOnly));spDevice->EndScene();const auto gpu=Pixels(target,read);
            unsigned nonempty=0,different=0;float maxError=0;
            for(size_t i=0;i<cpu.size()&&i<gpu.size();++i){nonempty+=cpu[i]!=0;const float e=std::fabs(cpu[i]-gpu[i]);if(e>maxError)maxError=e;different+=!std::isfinite(gpu[i])||e>2e-5f;}
            Check(cpu.size()==128*128*4&&gpu.size()==cpu.size()&&nonempty>200,"actual selected wheel points produce non-vacuous GPU output");
            Check(different==0,"GPU world XYZ/blur and coverage match CPU scale-rotate-translate with ordered slots");
            std::printf("wheel z=%u ordering=%u maxError=%g mismatchedLanes=%u fallback=%u nativeGroups=%u\n",zOnly,ordering,maxError,different,suFallback,suInstanced);
        }
        shader->Release();
    }
    Check(suFallback==8&&suInstanced==2,"actual programs exercise both fallback and native consumers");
    VirtualFree(storage,0,MEM_RELEASE);read->Release();target->Release();ib->Release();vb->Release();refVb->Release();actualDecl->Release();refDecl->Release();referenceVS->Release();observation->Release();white->Release();spDevice->Release();api->Release();DestroyWindow(window);
    std::printf("PCWheelShader: %u checks, %u failures\n",suChecks,suFailures);return suFailures?1:0;
}
