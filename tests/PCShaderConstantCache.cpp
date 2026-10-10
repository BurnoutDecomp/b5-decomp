#include "pc/gcm/renderengine/ShaderConstantCache.h"
#include "pc/gcm/renderengine/SamplerStateCache.h"
#include <cstdio>
#include <cstring>

static int checks, failures;
static void Check(bool lbPass, const char* lpcName)
{
    ++checks;
    if (!lbPass) { ++failures; std::printf("FAIL %s\n", lpcName); }
}

struct FakeDevice
{
    float maVertex[256][4] = {}, maPixel[224][4] = {};
    unsigned muCalls = 0;
    bool mbFail = false;
    DWORD muSamplerValue = 0;
    HRESULT SetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE state, DWORD value)
    {
        ++muCalls;
        if (mbFail || (sampler >= 16 && (sampler < D3DDMAPSAMPLER || sampler > D3DVERTEXTEXTURESAMPLER3)) ||
            state < D3DSAMP_ADDRESSU || state > D3DSAMP_DMAPOFFSET) return D3DERR_INVALIDCALL;
        muSamplerValue = value;
        return S_OK;
    }
    HRESULT Set(bool lbPixel, UINT luFirst, const float* lpData, UINT luCount)
    {
        ++muCalls;
        if (mbFail || !lpData || luFirst >= (lbPixel ? 224u : 256u) ||
            luCount > (lbPixel ? 224u : 256u) - luFirst) return D3DERR_INVALIDCALL;
        std::memcpy((lbPixel ? maPixel : maVertex)[luFirst], lpData, luCount * 16u);
        return S_OK;
    }
    HRESULT SetVertexShaderConstantF(UINT a, const float* b, UINT c) { return Set(false,a,b,c); }
    HRESULT SetPixelShaderConstantF(UINT a, const float* b, UINT c) { return Set(true,a,b,c); }
};

int main()
{
    using namespace renderengine;
    PCShaderConstantCache cache;
    FakeDevice device, other;
    float values[128][4];
    for (UINT i=0; i<128; ++i)
        for (UINT j=0; j<4; ++j) values[i][j] = static_cast<float>(i*4+j)/17.0f;
    Check(SUCCEEDED(cache.Set(&device,false,0,&values[0][0],128)), "first skin upload succeeds");
    for (UINT i=0; i<100; ++i) cache.Set(&device,false,0,&values[0][0],128);
    Check(device.muCalls==1 && cache.mStatistics.muSkipped==100, "unchanged skin is uploaded only once");
    values[127][3] += 0.5f;
    cache.Set(&device,false,0,&values[0][0],128);
    Check(device.muCalls==2 && device.maVertex[127][3]==values[127][3], "in-place source mutation is detected");
    const float gui[4] = {1,0,0.5f,1};
    cache.Set(&device,true,0,gui,1);
    Check(device.muCalls==3, "pixel and vertex registers are independent");
    cache.Set(&device,false,31,gui,1);
    cache.Set(&device,false,0,&values[0][0],128);
    Check(device.muCalls==5 && !std::memcmp(device.maVertex,values,sizeof(values)), "partial overwrite invalidates an overlapping larger upload");
    cache.Set(&device,false,64,&values[64][0],32);
    Check(device.muCalls==5, "known subranges can skip an upload");
    cache.Invalidate();
    cache.Set(&device,false,64,&values[64][0],32);
    Check(device.muCalls==6, "device reset invalidation forces upload");
    device.mbFail=true;
    values[0][0] += 0.25f;
    Check(FAILED(cache.Set(&device,false,0,&values[0][0],128)), "failed native call propagates");
    device.mbFail=false;
    cache.Set(&device,false,0,&values[0][0],128);
    Check(device.muCalls==8 && device.maVertex[0][0]==values[0][0], "failed values are retried rather than cached");
    cache.Set(&other,false,0,&values[0][0],128);
    Check(other.muCalls==1, "new device never inherits another device's register shadow");
    cache.Set(&device,false,0,&values[0][0],128);
    Check(device.muCalls==9, "switching back to a device also re-establishes state");
    Check(FAILED(cache.Set(&device,true,223,&values[0][0],2)), "invalid range retains native validation");
    Check(FAILED(cache.Set(&device,true,0,nullptr,1)), "null data retains native validation");
    Check(FAILED(cache.Set(static_cast<FakeDevice*>(nullptr),false,0,gui,1)), "null device is rejected");

    PCSamplerStateCache samplers;
    device.muCalls = other.muCalls = 0;
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    for (UINT i=0; i<100; ++i) samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    Check(device.muCalls==1 && samplers.mStatistics.muSkipped==100, "unchanged material sampler uploads once");
    samplers.Set(&device,0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
    samplers.Set(&device,1,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    samplers.Set(&device,D3DDMAPSAMPLER,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    samplers.Set(&device,D3DVERTEXTEXTURESAMPLER0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    samplers.Set(&device,D3DVERTEXTEXTURESAMPLER3,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    Check(device.muCalls==6, "sampler fields and all native sampler ranges are independent");
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    Check(device.muCalls==8 && device.muSamplerValue==D3DTADDRESS_WRAP, "GUI clamp followed by world wrap is restored");
    samplers.Invalidate();
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    Check(device.muCalls==9, "sampler reset invalidation forces upload");
    device.mbFail=true;
    Check(FAILED(samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP)), "failed sampler write propagates");
    device.mbFail=false;
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    Check(device.muCalls==11 && device.muSamplerValue==D3DTADDRESS_CLAMP, "failed sampler value is retried");
    samplers.Set(&other,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    samplers.Set(&device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
    Check(other.muCalls==1 && device.muCalls==12, "sampler shadows never leak across device changes");
    Check(FAILED(samplers.Set(&device,16,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP)), "invalid sampler preserves native validation");
    Check(FAILED(samplers.Set(&device,0,static_cast<D3DSAMPLERSTATETYPE>(14),0)), "invalid sampler field preserves native validation");
    Check(FAILED(samplers.Set(static_cast<FakeDevice*>(nullptr),0,D3DSAMP_ADDRESSU,0)), "null sampler device is rejected");

    // Real D3D9 register state, including a GUI write between world/skin draws.
    HWND window=CreateWindowA("STATIC","Shader constants",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* native=nullptr;
    D3DPRESENT_PARAMETERS pp={};
    pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow=window;
    pp.BackBufferWidth=64; pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Check(d3d && SUCCEEDED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&native)), "real D3D9 device available");
    if (native)
    {
        gPCShaderConstantCache.Invalidate();
        gPCSamplerStateCache.Invalidate();
        for (UINT pass=0; pass<3; ++pass)
        {
            PCSetVertexShaderConstantF(native,0,&values[0][0],128);
            PCSetPixelShaderConstantF(native,0,gui,1);
            PCSetVertexShaderConstantF(native,0,&values[0][0],128);
            float actual[128][4]={}, pixel[4]={};
            Check(SUCCEEDED(native->GetVertexShaderConstantF(0,&actual[0][0],128)) && !std::memcmp(actual,values,sizeof(values)), "world skin values survive mixed stage writes");
            Check(SUCCEEDED(native->GetPixelShaderConstantF(0,pixel,1)) && !std::memcmp(pixel,gui,sizeof(gui)), "GUI pixel constants reach native device");
            PCSetVertexShaderConstantF(native,127,gui,1);
            PCSetVertexShaderConstantF(native,0,&values[0][0],128);
            Check(SUCCEEDED(native->GetVertexShaderConstantF(0,&actual[0][0],128)) && !std::memcmp(actual,values,sizeof(values)), "native overlapping writes are restored correctly");
            const DWORD addresses[] = {D3DTADDRESS_WRAP, D3DTADDRESS_CLAMP, D3DTADDRESS_WRAP};
            for (DWORD address : addresses)
            {
                PCSetSamplerState(native,0,D3DSAMP_ADDRESSU,address);
                PCSetSamplerState(native,0,D3DSAMP_ADDRESSU,address);
                DWORD actualAddress=0;
                Check(SUCCEEDED(native->GetSamplerState(0,D3DSAMP_ADDRESSU,&actualAddress)) && actualAddress==address,
                      "native sampler follows world GUI world transitions");
            }
        }
        gPCShaderConstantCache.Invalidate();
        gPCSamplerStateCache.Invalidate();
        native->Release();
    }
    if (d3d) d3d->Release();
    if (window) DestroyWindow(window);
    std::printf("PCShaderConstantCache: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
