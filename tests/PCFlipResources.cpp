#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <vector>
#include <cstdio>
#include <cstring>
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/TextureUploadPCLeaf.h"

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement=1;
             __declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1; }
namespace renderengine { IDirect3DDevice9* gDevice=nullptr; }
namespace CgsDev::Log { void WriteToLog(const char* text) {std::fputs(text,stdout);} }
static int checks,failures;
static void Check(bool result,const char* label) {++checks;if(!result){++failures;std::printf("FAIL %s\n",label);}}
using renderengine::Texture;
using namespace renderengine::TextureUploadPC;
static ID3DBlob* Compile(const char* code,const char* target)
{
    ID3DBlob *result=nullptr,*error=nullptr;
    const HRESULT hr=D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,"main",target,0,0,&result,&error);
    if(FAILED(hr))std::printf("shader %08x %s\n",unsigned(hr),error?static_cast<char*>(error->GetBufferPointer()):"");
    if(error)error->Release();return result;
}
struct Sampler
{
    IDirect3DDevice9* device;
    IDirect3DVertexShader9* vs=nullptr;
    IDirect3DPixelShader9 *ps[3]={};
    IDirect3DVertexDeclaration9* declaration=nullptr;
    IDirect3DSurface9 *target=nullptr,*readback=nullptr;
    explicit Sampler(IDirect3DDevice9* d):device(d)
    {
        ID3DBlob* code=Compile("float4 main(float4 p:POSITION):POSITION{return p;}","vs_3_0");
        if(code){device->CreateVertexShader(static_cast<DWORD*>(code->GetBufferPointer()),&vs);code->Release();}
        const char* programs[]={
            "float4 coord:register(c0);sampler2D tex:register(s0);float4 main():COLOR{return tex2Dlod(tex,coord);}",
            "float4 coord:register(c0);samplerCUBE tex:register(s0);float4 main():COLOR{return texCUBElod(tex,coord);}",
            "float4 coord:register(c0);sampler3D tex:register(s0);float4 main():COLOR{return tex3Dlod(tex,coord);}"};
        for(unsigned i=0;i<3;++i){code=Compile(programs[i],"ps_3_0");if(code){device->CreatePixelShader(static_cast<DWORD*>(code->GetBufferPointer()),&ps[i]);code->Release();}}
        D3DVERTEXELEMENT9 elements[]={{0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},D3DDECL_END()};
        device->CreateVertexDeclaration(elements,&declaration);
        device->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
        device->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr);
    }
    bool Pixel(IDirect3DBaseTexture9* texture,unsigned kind,const float* coord,DWORD expected,DWORD mask=~DWORD(0))
    {
        if(!vs||!ps[kind]||!declaration||!target||!readback||!texture)return false;
        device->SetDepthStencilSurface(nullptr);device->SetRenderTarget(0,target);
        device->SetVertexShader(vs);device->SetPixelShader(ps[kind]);device->SetVertexDeclaration(declaration);
        device->SetRenderState(D3DRS_ZENABLE,FALSE);device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE,FALSE);device->SetRenderState(D3DRS_COLORWRITEENABLE,15);
        device->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
        device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_POINT);device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE);
        device->SetTexture(0,texture);device->SetPixelShaderConstantF(0,coord,1);
        const float quad[][4]={{-1,1,.5f,1},{1,1,.5f,1},{-1,-1,.5f,1},{1,-1,.5f,1}};
        device->BeginScene();device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0);
        HRESULT hr=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(quad[0]));device->EndScene();
        if(SUCCEEDED(hr))hr=device->GetRenderTargetData(target,readback);
        D3DLOCKED_RECT lock{};bool good=SUCCEEDED(hr)&&SUCCEEDED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        if(good){for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)
            good&=(reinterpret_cast<DWORD*>(static_cast<char*>(lock.pBits)+y*lock.Pitch)[x]&mask)==(expected&mask);
            readback->UnlockRect();}
        if(FAILED(hr))std::printf("sample draw/readback hr=%08x\n",unsigned(hr));
        device->SetTexture(0,nullptr);return good;
    }
    ~Sampler(){if(vs)vs->Release();for(auto* p:ps)if(p)p->Release();if(declaration)declaration->Release();if(target)target->Release();if(readback)readback->Release();}
};
static DWORD Colour(unsigned face,unsigned mip) {return 0xFF000000|(0x20+face*27)<<16|(0x30+mip*41)<<8|0x67;}
static void TestDevice(bool extended)
{
    HWND window=CreateWindowA("STATIC","Flip resource checks",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* api=nullptr;IDirect3D9Ex* exApi=nullptr;IDirect3DDevice9* device=nullptr;IDirect3DDevice9Ex* exDevice=nullptr;
    D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=extended?D3DSWAPEFFECT_FLIPEX:D3DSWAPEFFECT_COPY;
    params.BackBufferCount=extended?2:1;params.BackBufferWidth=params.BackBufferHeight=64;params.BackBufferFormat=D3DFMT_X8R8G8B8;
    params.hDeviceWindow=window;params.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr;
    if(extended){hr=Direct3DCreate9Ex(D3D_SDK_VERSION,&exApi);api=exApi;if(SUCCEEDED(hr))hr=exApi->CreateDeviceEx(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,nullptr,&exDevice);device=exDevice;}
    else{api=Direct3DCreate9(D3D_SDK_VERSION);hr=api?api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&params,&device):E_FAIL;}
    Check(SUCCEEDED(hr)&&device,"native device created");if(!device)return;
    renderengine::gDevice=device;
    {
        Sampler sample(device);
        Texture regular{};Texture::Parameters p{};p.meType=Texture::E_TYPE_2D;p.muWidth=8;p.muHeight=4;p.muDepth=1;p.muNumLevels=4;p.miFormat=D3DFMT_A8R8G8B8;
        std::vector<DWORD> data;
        for(unsigned mip=0;mip<4;++mip)data.insert(data.end(),(8u>>mip?8u>>mip:1)*(4u>>mip?4u>>mip:1),Colour(0,mip));
        Texture::Create(&regular,&p,data.data());
        bool pixels=regular.mpD3DTexture!=nullptr;
        for(unsigned mip=0;mip<4;++mip){const float coord[]={.5f,.5f,0,float(mip)};pixels&=sample.Pixel(regular.mpD3DTexture,0,coord,Colour(0,mip));}
        Check(pixels,"production 2D loader preserves every mip on the GPU");
        Texture::Locked lock{};Texture::Lock(&regular,2,0,0,&lock);
        bool updated=lock.mpPixelData&&lock.muWidth==2&&lock.muHeight==1;
        if(updated){auto* words=static_cast<DWORD*>(lock.mpPixelData);words[0]=words[1]=0xFF112233;Texture::Unlock(&regular,&lock);}
        const float mip2[]={.5f,.5f,0,2},mip0[]={.5f,.5f,0,0};
        Check(updated&&sample.Pixel(regular.mpD3DTexture,0,mip2,0xFF112233)&&sample.Pixel(regular.mpD3DTexture,0,mip0,Colour(0,0)),
            "nonzero-mip runtime edits upload while preserving other levels");
        Texture::LockInfo read{};Texture::Lock(&regular,2,0,D3DLOCK_READONLY,&read);
        bool readonly=read.mpBits&&*static_cast<DWORD*>(read.mpBits)==0xFF112233;
        if(read.mpBits)Texture::Unlock(&regular,&read);
        Check(readonly,"lean read-only locks retain editable texture contents");
        Texture cube{};p.meType=Texture::E_TYPE_CUBE;p.muWidth=p.muHeight=4;p.muDepth=6;p.muNumLevels=3;
        data.clear();for(unsigned face=0;face<6;++face)for(unsigned mip=0;mip<3;++mip)data.insert(data.end(),(4u>>mip)*(4u>>mip),Colour(face,mip));
        Texture::Create(&cube,&p,data.data());
        const float axes[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        pixels=cube.mpD3DTexture!=nullptr;
        for(unsigned face=0;face<6;++face)for(unsigned mip=0;mip<3;++mip){float coord[]={axes[face][0],axes[face][1],axes[face][2],float(mip)};pixels&=sample.Pixel(cube.mpD3DTexture,1,coord,Colour(face,mip));}
        Check(pixels,"production cube loader preserves face-major data and every mip");
        Texture compressed{};p.muWidth=p.muHeight=8;p.muNumLevels=4;p.miFormat=D3DFMT_DXT1;
        const unsigned short palette[]={0xF800,0x07E0,0x001F,0xFFFF,0x07FF,0xFFE0};
        const DWORD rgb[]={0xFFFF0000,0xFF00FF00,0xFF0000FF,0xFFFFFFFF,0xFF00FFFF,0xFFFFFF00};
        std::vector<unsigned char> blocks;
        for(unsigned face=0;face<6;++face)for(unsigned mip=0;mip<4;++mip){unsigned edge=8>>mip,side=(edge+3)/4;
            for(unsigned block=0;block<side*side;++block){unsigned short colour=palette[(face+mip)%6];
                blocks.push_back(static_cast<unsigned char>(colour));blocks.push_back(static_cast<unsigned char>(colour>>8));
                blocks.insert(blocks.end(),6,0);}}
        Texture::Create(&compressed,&p,blocks.data());pixels=compressed.mpD3DTexture!=nullptr;
        for(unsigned face=0;face<6;++face)for(unsigned mip=0;mip<4;++mip){float coord[]={axes[face][0],axes[face][1],axes[face][2],float(mip)};
            pixels&=sample.Pixel(compressed.mpD3DTexture,1,coord,rgb[(face+mip)%6]);}
        Check(pixels,"compressed cube block rows and sub-four-pixel mips reach the GPU unchanged");
        Texture alpha{};p.meType=Texture::E_TYPE_2D;p.muWidth=5;p.muHeight=3;p.muDepth=1;p.muNumLevels=1;p.miFormat=D3DFMT_A8;
        const unsigned char alphaData[15]={17,17,17,17,17,128,128,128,128,128,240,240,240,240,240};
        Texture::Create(&alpha,&p,alphaData);
        bool alphaValid=true;for(unsigned row=0;row<3;++row){float coord[]={.5f,(row+.5f)/3,0,0};
            alphaValid&=sample.Pixel(alpha.mpD3DTexture,0,coord,DWORD(alphaData[row*5])<<24,0xFF000000);}
        Check(alphaValid,"padded single-channel texture rows retain GUI alpha");
        Texture volume{};p.meType=Texture::E_TYPE_VOLUME;p.muWidth=4;p.muHeight=2;p.muDepth=4;p.muNumLevels=1;
        p.miFormat=D3DFMT_A8R8G8B8;
        Texture::Create(&volume,&p,nullptr);Texture::Lock(&volume,0,0,0,&lock);
        updated=lock.mpPixelData&&lock.muSliceStride&&lock.muVolumeDepth==4;
        if(updated){for(unsigned z=0;z<4;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x)
            reinterpret_cast<DWORD*>(static_cast<char*>(lock.mpPixelData)+z*lock.muSliceStride+y*lock.muStride)[x]=Colour(z,0);
            Texture::Unlock(&volume,&lock);}
        pixels=updated;for(unsigned z=0;z<4;++z){float coord[]={.5f,.5f,(z+.5f)/4,0};pixels&=sample.Pixel(volume.mpD3DTexture,2,coord,Colour(z,0));}
        Check(pixels,"runtime colour-volume edits preserve row and slice pitches");
        Texture volumeMips{};p.muWidth=p.muHeight=p.muDepth=4;p.muNumLevels=3;
        data.clear();
        for(unsigned mip=0;mip<3;++mip)for(unsigned z=0;z<(4u>>mip);++z)
            data.insert(data.end(),(4u>>mip)*(4u>>mip),Colour(z,mip));
        Texture::Create(&volumeMips,&p,data.data());pixels=volumeMips.mpD3DTexture!=nullptr;
        for(unsigned mip=0;mip<3;++mip)for(unsigned z=0;z<(4u>>mip);++z){
            float coord[]={.5f,.5f,(z+.5f)/(4u>>mip),float(mip)};
            pixels&=sample.Pixel(volumeMips.mpD3DTexture,2,coord,Colour(z,mip));}
        Check(pixels,"production volume loader preserves every mip and slice on the GPU");
        Texture::Lock(&volumeMips,1,0,0,&lock);
        updated=lock.mpPixelData&&lock.muWidth==2&&lock.muHeight==2&&lock.muVolumeDepth==2;
        if(updated){for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x)
            reinterpret_cast<DWORD*>(static_cast<char*>(lock.mpPixelData)+z*lock.muSliceStride+y*lock.muStride)[x]=Colour(z+3,1);
            Texture::Unlock(&volumeMips,&lock);}
        pixels=updated;
        for(unsigned mip=0;mip<3;++mip)for(unsigned z=0;z<(4u>>mip);++z){
            float coord[]={.5f,.5f,(z+.5f)/(4u>>mip),float(mip)};
            pixels&=sample.Pixel(volumeMips.mpD3DTexture,2,coord,Colour(mip==1?z+3:z,mip));}
        Check(pixels,"runtime volume sublevel edits upload every slice without changing other mips");
        if(extended){Texture::Locked first{},second{};Texture::Lock(&regular,0,0,0,&first);Texture::Lock(&regular,1,0,0,&second);
            bool overlap=first.mpPixelData&&second.mpPixelData;
            if(overlap){for(unsigned y=0;y<4;++y)for(unsigned x=0;x<8;++x)reinterpret_cast<DWORD*>(static_cast<char*>(first.mpPixelData)+y*first.muStride)[x]=0xFF554433;
                for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x)reinterpret_cast<DWORD*>(static_cast<char*>(second.mpPixelData)+y*second.muStride)[x]=0xFF997755;
                Texture::Unlock(&regular,&first);
                overlap&=sample.Pixel(regular.mpD3DTexture,0,mip0,Colour(0,0));
                Texture::Unlock(&regular,&second);const float mip1[]={.5f,.5f,0,1};
                overlap&=sample.Pixel(regular.mpD3DTexture,0,mip0,0xFF554433)&&sample.Pixel(regular.mpD3DTexture,0,mip1,0xFF997755);}
            Check(overlap,"overlapping mip edits publish only after the last CPU lock is released");}
        if(extended){D3DPRESENT_PARAMETERS reset=params;reset.BackBufferWidth=96;reset.BackBufferHeight=80;
            const HRESULT resetResult=exDevice->ResetEx(&reset,nullptr);
            Check(SUCCEEDED(resetResult)&&sample.Pixel(regular.mpD3DTexture,0,mip2,0xFF112233),
                "primary flip resize preserves loaded texture data and shaders");}
        Shadow* shadow=Acquire(regular.mpD3DTexture);
        Check((shadow!=nullptr)==extended,"only 9Ex textures carry staging ownership");
        Texture::Destroy(&regular);
        if(shadow){const ULONG retained=shadow->AddRef();shadow->Release();const ULONG last=shadow->Release();
            Check(retained==2&&last==0,"destroying the GPU texture releases its staging owner without a reference cycle");}
        Texture::Destroy(&cube);Texture::Destroy(&volume);Texture::Destroy(&volumeMips);Texture::Destroy(&compressed);Texture::Destroy(&alpha);
    }
    renderengine::gDevice=nullptr;device->Release();api->Release();DestroyWindow(window);
}
int main(){TestDevice(false);TestDevice(true);std::printf("PCFlipResources: %d checks, %d failures\n",checks,failures);return failures?1:0;}
