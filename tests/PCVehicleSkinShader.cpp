#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "types.hpp"
static IDirect3DDevice9* spDevice;
static unsigned suChecks, suFailures;
static IDirect3DDevice9* Dev() { return spDevice; }
static void LogOnce(const char*,const char*) {}
static void LogUnsetShaderConstantOnce() {}
namespace renderengine {
void PCSetVertexShaderConstantF(IDirect3DDevice9* d,u32 start,const float* data,u32 count) { d->SetVertexShaderConstantF(start,data,count); }
void PCSetPixelShaderConstantF(IDirect3DDevice9* d,u32 start,const float* data,u32 count) { d->SetPixelShaderConstantF(start,data,count); }
}
#include "skin_shader.inc"
static void Check(bool good,const char* label) {
    ++suChecks; if(!good) { ++suFailures; std::printf("FAIL %s\n",label); }
}
// Matches the shader's actual semantics: FLOAT3 position/normal/tangent,
// twoFLOAT2 texture sets, UBYTE4 indices and UBYTE4N weights. The deliberately
// nonzero Z/W indices and weights detect an accidental four-way gather.
struct Vertex {
    float pos[3],normal[3],tangent[3],uv[2],uv1[2];
    unsigned char indices[4],weights[4];
};
static bool Close(float a,float b) { return std::fabs(a-b)<2e-5f; }
int main() {
    HWND window=CreateWindowA("STATIC","native vehicle skin gather",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp={}; pp.Windowed=TRUE; pp.hDeviceWindow=window; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    if(!api || FAILED(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&spDevice))) return 2;
    IDirect3DVertexShader9* vertexShader=nullptr;
    Check(SUCCEEDED(spDevice->CreateVertexShader(reinterpret_cast<const DWORD*>(skinVS),&vertexShader)),"unchanged installed Vehicle_PaintGloss_Damaged VS loads");
    // TEXCOORD0.xyz = ViewPosition - world position; .w = blended scratch.
    // TEXCOORD2.xy carries our original XY, allowing one pixel to reveal delta
    // independently of interpolation/raster location. Only this observation PS
    // is synthetic; the VS/influences/constant publication are production.
    const char* pixelSource=
        "float4 main(float4 eye:TEXCOORD0,float4 uv:TEXCOORD2):COLOR0 { return float4(-eye.xy-uv.xy,-eye.z-0.2,eye.w); }";
    ID3DBlob* pixelCode=nullptr; ID3DBlob* errors=nullptr;
    HRESULT hr=D3DCompile(pixelSource,std::strlen(pixelSource),nullptr,nullptr,nullptr,"main","ps_3_0",0,0,&pixelCode,&errors);
    Check(SUCCEEDED(hr),"observation pixel shader compiles");
    if(errors) errors->Release(); if(!pixelCode) return 2;
    IDirect3DPixelShader9* pixelShader=nullptr;
    Check(SUCCEEDED(spDevice->CreatePixelShader(static_cast<const DWORD*>(pixelCode->GetBufferPointer()),&pixelShader)),"observation pixel shader loads"); pixelCode->Release();
    const D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},{0,24,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_TANGENT,0},
        {0,36,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,0},{0,44,D3DDECLTYPE_FLOAT2,0,D3DDECLUSAGE_TEXCOORD,1},
        {0,52,D3DDECLTYPE_UBYTE4,0,D3DDECLUSAGE_BLENDINDICES,0},{0,56,D3DDECLTYPE_UBYTE4N,0,D3DDECLUSAGE_BLENDWEIGHT,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* declaration=nullptr;
    Check(SUCCEEDED(spDevice->CreateVertexDeclaration(elements,&declaration)),"actual index/weight normalization semantics bind");
    IDirect3DSurface9* target=nullptr; IDirect3DSurface9* readback=nullptr;
    Check(SUCCEEDED(spDevice->CreateRenderTarget(64,64,D3DFMT_A32B32G32R32F,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr)),"float4 GPU observation target");
    Check(SUCCEEDED(spDevice->CreateOffscreenPlainSurface(64,64,D3DFMT_A32B32G32R32F,D3DPOOL_SYSTEMMEM,&readback,nullptr)),"float4 readback target");
    if(!target || !readback) return 2;
    spDevice->SetDepthStencilSurface(nullptr); spDevice->SetRenderTarget(0,target);
    D3DVIEWPORT9 viewport={0,0,64,64,0,1}; spDevice->SetViewport(&viewport);
    spDevice->SetRenderState(D3DRS_ZENABLE,FALSE); spDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    spDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE); spDevice->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    spDevice->SetVertexShader(vertexShader); spDevice->SetPixelShader(pixelShader); spDevice->SetVertexDeclaration(declaration);
    // Original modified projection encodes Z/W through c143.x/y/z/w.
    float constants[256][4]={}; constants[140][0]=1; constants[141][1]=1; constants[142][2]=1;
    constants[143][0]=1; constants[143][3]=1;
    constants[148][0]=constants[149][1]=constants[150][2]=constants[151][3]=1;
    renderengine::WorldShaderConstants_Set(false,0,constants,256);
    auto* handle=static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(0x14000000),4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!handle || reinterpret_cast<uintptr_t>(handle)>0xffffffff) return 2;
    // Actual rebuilt descriptor for g_verletOffsets: c0,count128, float4,VS.
    handle[0]=0; handle[1]=2; handle[2]=0; handle[3]=128;
    u32 block[4]={1,0,0,static_cast<u32>(reinterpret_cast<uintptr_t>(handle))};
    const unsigned pairs[][2]={{0,127},{127,0},{1,126},{63,64},{64,63},{4,117},{117,4},{127,127}};
    const unsigned weights[][2]={{255,0},{0,255},{64,191},{191,64}};
    for(const auto& pair:pairs) for(const auto& weight:weights) {
        float palette[128][4];
        for(unsigned row=0;row<128;++row) {
            palette[row][0]=float(int(row%11)-5)*.025f;
            palette[row][1]=float(int(row%7)-3)*.025f;
            palette[row][2]=float(row%5)*.015f;
            palette[row][3]=float(row)*.005f;
        }
        void* sources[]={palette}; DispatchExternalBlock(block,sources,false);
        float native[128][4]={}; spDevice->GetVertexShaderConstantF(0,&native[0][0],128);
        Check(std::memcmp(native,palette,sizeof(palette))==0,"production external dispatcher publishes every row0..127");
        Vertex vertices[4]={}; const float xy[][2]={{-.6f,.6f},{.6f,.6f},{-.6f,-.6f},{.6f,-.6f}};
        for(unsigned v=0;v<4;++v) {
            vertices[v].pos[0]=vertices[v].uv[0]=xy[v][0]; vertices[v].pos[1]=vertices[v].uv[1]=xy[v][1]; vertices[v].pos[2]=.2f;
            vertices[v].normal[2]=1; vertices[v].tangent[0]=1;
            vertices[v].indices[0]=static_cast<unsigned char>(pair[0]); vertices[v].indices[1]=static_cast<unsigned char>(pair[1]);
            vertices[v].indices[2]=200; vertices[v].indices[3]=201;
            vertices[v].weights[0]=static_cast<unsigned char>(weight[0]); vertices[v].weights[1]=static_cast<unsigned char>(weight[1]);
            vertices[v].weights[2]=99; vertices[v].weights[3]=77;
        }
        spDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0); spDevice->BeginScene();
        hr=spDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)); spDevice->EndScene();
        Check(SUCCEEDED(hr),"actual vehicle vertex shader draws indexed influence pair");
        Check(SUCCEEDED(spDevice->GetRenderTargetData(target,readback)),"GPU influence output read back");
        D3DLOCKED_RECT lock={}; readback->LockRect(&lock,nullptr,D3DLOCK_READONLY);
        const auto* pixel=reinterpret_cast<const float*>(static_cast<const char*>(lock.pBits)+32*lock.Pitch)+32*4;
        bool matches=true;
        for(unsigned lane=0;lane<4;++lane) {
            float expected=palette[pair[1]][lane]*(float(weight[1])/255.0f)+palette[pair[0]][lane]*(float(weight[0])/255.0f);
            if(!Close(pixel[lane],expected)) {
                matches=false; std::printf("pair%u/%u weights%u/%u lane%u actual%.7f expected%.7f\n",pair[0],pair[1],weight[0],weight[1],lane,pixel[lane],expected);
            }
        }
        Check(matches,"GPU XYZ/scratchW equals original ordered two-row weighted gather"); readback->UnlockRect();
    }
    VirtualFree(handle,0,MEM_RELEASE); readback->Release(); target->Release(); declaration->Release(); vertexShader->Release(); pixelShader->Release(); spDevice->Release(); api->Release(); DestroyWindow(window);
    std::printf("PCVehicleSkinShader: %u checks, %u failures\n",suChecks,suFailures); return suFailures?1:0;
}
