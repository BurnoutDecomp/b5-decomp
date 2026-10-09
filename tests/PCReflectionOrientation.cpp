// Native pixels at all cube-face edges, using production EnvironmentMap cameras.
#include <Windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <initializer_list>
#include <vector>
#include "GameSource/World/EnvironmentMap/BrnEnvironmentMap.h"
#include "rw/math/vpu/vector3_operation.h"
#include "pc/gcm/renderengine/EnvironmentMapPCLeaf.h"
#include "pc/gcm/renderengine/ReflectionDistancePCLeaf.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
using rw::math::vpu::Vector4;
static IDirect3DDevice9* spDevice;
static IDirect3DDevice9* Dev() { return spDevice; }
static std::vector<void*> saAllocations;
void* AllocateTestState(void* lpOut,const void* lpDescriptor)
{
    const auto* lpEntries=static_cast<const renderengine::ResourceDescriptorEntry*>(lpDescriptor);
    if(lpEntries[0].muSize!=sizeof(renderengine::RasterizerState)) std::abort();
    void* lpMemory=std::calloc(1,lpEntries[0].muSize);saAllocations.push_back(lpMemory);
    static_cast<void**>(lpOut)[0]=lpMemory;return lpOut;
}
#include "reflection_orientation.inc"

static u32 suChecks = 0, suFailures = 0;
static void Require(HRESULT lhResult, const char* lpcName)
{
    if (FAILED(lhResult)) { std::printf("GPU FAILURE %s hr=%08X\n", lpcName, unsigned(lhResult)); std::exit(2); }
}
static void Check(bool lbResult, const char* lpcName)
{
    ++suChecks;
    if (!lbResult) { ++suFailures; if (suFailures <= 16) std::printf("FAIL %s\n", lpcName); }
}
static DWORD Colour(f32 lfX, f32 lfY, f32 lfZ)
{
    return D3DCOLOR_ARGB(255,u32(128.f+lfX*9.f),u32(128.f+lfY*9.f),u32(128.f+lfZ*9.f));
}
struct Vertex { f32 mfX, mfY, mfZ; DWORD mColour; };
static void DrawBox(IDirect3DDevice9* lpDevice, const rw::math::vpu::Vector3& lEye)
{
    // One continuous analytic colour field on a box surrounding the cube centre.
    // Face identity colours alone cannot expose a mirrored face.
    const f32 lafCorners[8][3] = {
        {-10,-10,-10},{10,-10,-10},{-10,10,-10},{10,10,-10},
        {-10,-10,10},{10,-10,10},{-10,10,10},{10,10,10} };
    // Inward-facing geometry: the camera sees the front of every surrounding face.
    const u32 lauFaces[6][4] = { {1,5,3,7}, {4,0,6,2}, {2,3,6,7},
                               {4,5,0,1}, {5,4,7,6}, {0,1,2,3} };
    for (const auto& laFace : lauFaces)
    {
        Vertex laVertices[4];
        for (u32 lu=0;lu<4;++lu)
        {
            const f32* lp=lafCorners[laFace[lu]];
            laVertices[lu]={lp[0]+lEye.x,lp[1]+lEye.y,lp[2]+lEye.z,Colour(lp[0],lp[1],lp[2])};
        }
        Require(lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,laVertices,sizeof(Vertex)),"world field");
    }
}
static DWORD Pixel(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpSource, IDirect3DSurface9* lpRead)
{
    Require(lpDevice->GetRenderTargetData(lpSource,lpRead),"readback");
    D3DLOCKED_RECT lLock = {};
    Require(lpRead->LockRect(&lLock,nullptr,D3DLOCK_READONLY),"lock");
    DWORD lPixel;
    std::memcpy(&lPixel,static_cast<char*>(lLock.pBits)+32*lLock.Pitch+32*4,4);
    lpRead->UnlockRect();
    return lPixel;
}
static u32 Difference(DWORD lA, DWORD lB)
{
    u32 luMax=0;
    for (u32 luShift : {0u,8u,16u})
    {
        const u32 luDiff=u32(std::abs(s32((lA>>luShift)&255)-s32((lB>>luShift)&255)));
        if (luDiff>luMax) luMax=luDiff;
    }
    return luMax;
}
int main()
{
    HWND lWindow=CreateWindowA("STATIC","cube orientation regression",WS_OVERLAPPEDWINDOW,
        0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* lpApi=Direct3DCreate9(D3D_SDK_VERSION);
    if (!lpApi || !lWindow) return 2;
    D3DPRESENT_PARAMETERS lPresent={};
    lPresent.Windowed=TRUE;lPresent.hDeviceWindow=lWindow;lPresent.SwapEffect=D3DSWAPEFFECT_DISCARD;
    lPresent.BackBufferWidth=lPresent.BackBufferHeight=64;lPresent.BackBufferFormat=D3DFMT_X8R8G8B8;
    IDirect3DDevice9* lpDevice=nullptr;
    Require(lpApi->CreateDevice(0,D3DDEVTYPE_HAL,lWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,
        &lPresent,&lpDevice),"device");
    spDevice=lpDevice;
    CgsRasterizerStateFactory lRasterFactory;lRasterFactory.Construct(nullptr);
    IDirect3DCubeTexture9* lpCube=nullptr;
    IDirect3DSurface9 *lpSample=nullptr,*lpRead=nullptr;
    Require(lpDevice->CreateCubeTexture(128,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&lpCube,nullptr),"cube");
    Require(lpDevice->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&lpSample,nullptr),"sample target");
    Require(lpDevice->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpRead,nullptr),"read surface");
    lpDevice->SetDepthStencilSurface(nullptr);
    lpDevice->SetRenderState(D3DRS_ZENABLE,FALSE);
    lpDevice->SetRenderState(D3DRS_LIGHTING,FALSE);
    lpDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    D3DMATRIX lIdentity={};lIdentity._11=lIdentity._22=lIdentity._33=lIdentity._44=1.f;
    lpDevice->SetTransform(D3DTS_WORLD,&lIdentity);lpDevice->SetTransform(D3DTS_VIEW,&lIdentity);

    // A fixture consumer uses the actual D3D9 samplerCUBE lookup at the vehicle unit.
    const char* lpcPs="samplerCUBE cubeSampler:register(s13); float4 direction:register(c0);"
                      "float4 main():COLOR0 { return texCUBE(cubeSampler,direction.xyz); }";
    ID3DBlob *lpCode=nullptr,*lpErrors=nullptr;
    Require(D3DCompile(lpcPs,std::strlen(lpcPs),nullptr,nullptr,nullptr,"main","ps_3_0",0,0,&lpCode,&lpErrors),"cube sample shader");
    IDirect3DPixelShader9* lpPs=nullptr;
    Require(lpDevice->CreatePixelShader(static_cast<const DWORD*>(lpCode->GetBufferPointer()),&lpPs),"pixel shader");
    lpCode->Release();if(lpErrors)lpErrors->Release();
    BrnGraphics::EnvironmentMap lEnv={};
    lEnv.Prepare();
    // Exercise the real camera query planes and projection at a distance beyond
    // the original 75 m range, including a live reset after a positive override.
    for (f32 lfDistance : {0.0f,500.0f,0.0f})
    {
        renderengine::EnvironmentMapDrawDistancePC()=lfDistance;
        lEnv.Update({0,0,0,0});
        for (u32 luFace=0;luFace<6;++luFace)
        {
            const CgsGraphics::Camera& lrCamera=lEnv.maEnvMapCameras[luFace];
            CgsGraphics::CameraRwFrustum lFrustum;
            lrCamera.GetFrustumPerspective(lFrustum,false);
            const auto& lrDirection=BrnGraphics::KAV_ENV_MAP_LOOK_DIRECTIONS[luFace];
            const auto& lrFar=lFrustum.maPlanes[1];
            const f32 lfFarSide=200.f*(lrFar.x*lrDirection.x+lrFar.y*lrDirection.y+lrFar.z*lrDirection.z)-lrFar.w;
            Check((lfFarSide>=0)==(lfDistance>0),"actual reflection query far plane responds to extension and reset");
            const auto& lrVp=lrCamera.GetViewProjectionMatrix();
            const f32 lfZ=200.f*(lrDirection.x*lrVp.xAxis.z+lrDirection.y*lrVp.yAxis.z+lrDirection.z*lrVp.zAxis.z)+lrVp.wAxis.z;
            const f32 lfW=200.f*(lrDirection.x*lrVp.xAxis.w+lrDirection.y*lrVp.yAxis.w+lrDirection.z*lrVp.zAxis.w)+lrVp.wAxis.w;
            Check((lfZ>=0 && lfZ<=lfW)==(lfDistance>0),"actual reflection projection includes distant geometry only when extended");
        }
    }
    u32 luWorstEdge=0,luWorstField=0;
    for (f32 lfDistance : {0.0f,500.0f})
    for (u32 luCameraPass=0;luCameraPass<=KU_SKY_PROJECTIONS;++luCameraPass)
    for (bool lbPassCull : {false,true})
    for (const rw::math::vpu::Vector3 lEye : { rw::math::vpu::Vector3{0,0,0,0}, rw::math::vpu::Vector3{37,11,-22,0} })
    {
        renderengine::EnvironmentMapDrawDistancePC()=lfDistance;
        lEnv.Update(lEye);
        lpDevice->SetTexture(13,nullptr);
        lpDevice->SetPixelShader(nullptr);
        lpDevice->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
        lpDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        lpDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        D3DDevice_SetRenderState_CullMode(lpDevice,lbPassCull ? CgsRasterizerStateFactory::GetState(KE_REFLECTION_CULL)->muCullMode : 0u);
        for (u32 luFace=0;luFace<6;++luFace)
        {
            IDirect3DSurface9* lpFace=nullptr;
            Require(lpCube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(luFace),0,&lpFace),"cube face");
            Require(lpDevice->SetRenderTarget(0,lpFace),"bind face");
            CgsGraphics::Camera lCamera=lEnv.maEnvMapCameras[luFace];
            if(luCameraPass>0)SkyProjection(luCameraPass-1,lCamera);
            const auto& lrMatrix=lCamera.GetViewProjectionMatrix();
            static_assert(sizeof(lrMatrix)==sizeof(D3DMATRIX),"native matrix extent");
            D3DMATRIX lProjection;std::memcpy(&lProjection,&lrMatrix,sizeof(lProjection));
            lpDevice->SetTransform(D3DTS_PROJECTION,&lProjection);
            lpDevice->Clear(0,nullptr,D3DCLEAR_TARGET,0xFF000000,1.f,0);
            lpDevice->BeginScene();DrawBox(lpDevice,lEye);lpDevice->EndScene();lpFace->Release();
        }
        lpDevice->SetRenderTarget(0,lpSample);lpDevice->SetTransform(D3DTS_PROJECTION,&lIdentity);
        lpDevice->SetPixelShader(lpPs);lpDevice->SetTexture(13,lpCube);
        lpDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        lpDevice->SetSamplerState(13,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(13,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
        lpDevice->SetSamplerState(13,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        lpDevice->SetSamplerState(13,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        lpDevice->SetSamplerState(13,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        const Vertex laQuad[]={ {-1,-1,.5f,0},{-1,1,.5f,0},{1,-1,.5f,0},{1,1,.5f,0} };
        const auto lSample=[&](const f32* lpDirection) {
            lpDevice->SetPixelShaderConstantF(0,lpDirection,1);
            lpDevice->BeginScene();
            Require(lpDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,laQuad,sizeof(Vertex)),"sample cube");
            lpDevice->EndScene();return Pixel(lpDevice,lpSample,lpRead);
        };
        // All twelve edges, five positions per edge, both sides of the face-selection boundary.
        for (u32 luA=0;luA<3;++luA)for (u32 luB=luA+1;luB<3;++luB)
        for (f32 lfA : {-1.f,1.f})for (f32 lfB : {-1.f,1.f})
        for (f32 lfAlong : {-.8f,-.4f,0.f,.4f,.8f})
        {
            const u32 luC=3-luA-luB;
            f32 lafD1[4]={},lafD2[4]={};
            lafD1[luA]=lfA*1.01f;lafD1[luB]=lfB;lafD1[luC]=lfAlong;
            lafD2[luA]=lfA;lafD2[luB]=lfB*1.01f;lafD2[luC]=lfAlong;
            const DWORD lP1=lSample(lafD1),lP2=lSample(lafD2);
            const u32 luEdge=Difference(lP1,lP2);
            if(luEdge>luWorstEdge)luWorstEdge=luEdge;
            Check(luEdge<=4,"adjacent faces sample continuous world field");
            const DWORD lExpected=Colour(lafD1[0]*10/1.01f,lafD1[1]*10/1.01f,lafD1[2]*10/1.01f);
            const u32 luField=Difference(lP1,lExpected);
            if(luField>luWorstField)luWorstField=luField;
            Check(luField<=4,"native cube lookup returns correct world direction");
        }
    }
    renderengine::EnvironmentMapDrawDistancePC()=0;
    lpDevice->SetTexture(13,nullptr);lpPs->Release();lpRead->Release();lpSample->Release();lpCube->Release();
    lpDevice->Release();lpApi->Release();DestroyWindow(lWindow);
    for(void* lpMemory : saAllocations)std::free(lpMemory);
    std::printf("worst cube edge jump=%u/255 field error=%u/255\n",luWorstEdge,luWorstField);
    std::printf("PCReflectionOrientation: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
