#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdlib>
#include "pc/gcm/renderengine/reflections/CubeHistory.h"
#include "pc/gcm/renderengine/DepthRange.h"
#include "pc/gcm/renderengine/reflections/GlassCapture.h"
#include "pc/gcm/renderengine/reflections/LightSubmission.h"
static u32 suChecks = 0, suFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{ ++suChecks; if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcName); } }
static void Require(HRESULT lhResult, const char* lpcName)
{ if (FAILED(lhResult)) { std::printf("GPU FAILURE %s %08x\n", lpcName, unsigned(lhResult)); std::exit(2); } }
namespace CgsDev::Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lp, const char*, int) { std::printf("ASSERT %s\n", lp); std::exit(3); }
void* EndAssert() { return nullptr; }
}
namespace CgsGraphics { void DispatchBin::HandleMemoryOverflow(u32) { std::exit(4); } }
#include "pc_reflection_capture.inc"
int main()
{
    using namespace CgsPC::Reflections;
    int lCaptureInterface=0,lMainInterface=0;
    Lights().mbEnabled=true;Lights().miDistanceMode=E_DISTANCE_FIXED;Lights().mfDrawDistance=500;
    {
        LightSubmissionScope lScope(&lCaptureInterface,{});
        Check(LightFadeDistanceSquared(&lCaptureInterface,440*440,62500)==48400,
            "capture corona alpha retains its original fade shape at an extended distance");
        Check(LightFadeDistanceSquared(&lCaptureInterface,500*500,62500)==62500,
            "capture corona alpha reaches zero at the independently configured cutoff");
        Check(LightFadeDistanceSquared(&lMainInterface,440*440,62500)==440*440,
            "capture distance does not alter main-view corona fading");
        Check(AcceptLight(&lCaptureInterface,{499,0,0},0,512,62500)
            && !AcceptLight(&lCaptureInterface,{501,0,0},0,512,62500)
            && !AcceptLight(&lCaptureInterface,{1,0,0},512,512,62500),
            "extended corona cutoff and capacity constrain the original writer");
        Lights().miDistanceMode=E_DISTANCE_RELATIVE;Lights().mfDrawDistanceScale=0.5f;
        Check(LightFadeDistanceSquared(&lCaptureInterface,110*110,62500)==48400,
            "relative capture corona fade follows the shorter live cutoff");
    }
    Check(LightFadeDistanceSquared(&lCaptureInterface,440*440,62500)==440*440,
        "leaving the capture scope restores original corona fading");
    Lights()={};
    HWND lWindow = CreateWindowA("STATIC", "reflection capture regression", WS_OVERLAPPEDWINDOW,
        0,0,32,32,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* lpApi = Direct3DCreate9(D3D_SDK_VERSION);
    if (!lpApi || !lWindow) return 2;
    D3DPRESENT_PARAMETERS lPresent = {};
    lPresent.Windowed=TRUE; lPresent.hDeviceWindow=lWindow; lPresent.SwapEffect=D3DSWAPEFFECT_DISCARD;
    lPresent.BackBufferWidth=lPresent.BackBufferHeight=32; lPresent.BackBufferFormat=D3DFMT_X8R8G8B8;
    IDirect3DDevice9* lpDevice = nullptr;
    Require(lpApi->CreateDevice(0,D3DDEVTYPE_HAL,lWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,
        &lPresent,&lpDevice), "device");
    IDirect3DCubeTexture9* lpCube = nullptr;
    Require(lpDevice->CreateCubeTexture(32,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&lpCube,nullptr),"cube");
    for (u32 luFace=0;luFace<6;++luFace) {
        IDirect3DSurface9* lpSurface = nullptr;
        Require(lpCube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(luFace),0,&lpSurface),"cube face");
        Require(lpDevice->ColorFill(lpSurface,nullptr,0xff000010u+luFace),"face colour"); lpSurface->Release();
    }
    CubeHistory lHistory;
    Check(lHistory.Begin(lpDevice,lpCube,true),"completed cube can be snapshotted");
    Check(lHistory.Resolve(lpCube)!=lpCube && lHistory.Resolve(lpCube)!=nullptr,"sampled texture differs from active target");
    IDirect3DBaseTexture9* lpBound = nullptr;
    lpDevice->GetTexture(13,&lpBound);
    Check(lpBound==lHistory.GetSnapshot(),"default environment sampler uses immutable history");
    if(lpBound)lpBound->Release();
    IDirect3DSurface9* lpReadback = nullptr;
    Require(lpDevice->CreateOffscreenPlainSurface(32,32,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&lpReadback,nullptr),"readback");
    for (u32 luFace=0;luFace<6;++luFace) {
        IDirect3DSurface9 *lpSource=nullptr,*lpSnapshot=nullptr;
        lpCube->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(luFace),0,&lpSource);
        lpDevice->ColorFill(lpSource,nullptr,0xffabcdefu);
        static_cast<IDirect3DCubeTexture9*>(lHistory.GetSnapshot())->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(luFace),0,&lpSnapshot);
        Require(lpDevice->GetRenderTargetData(lpSnapshot,lpReadback),"snapshot pixels");
        D3DLOCKED_RECT lLocked={}; lpReadback->LockRect(&lLocked,nullptr,D3DLOCK_READONLY);
        Check(*static_cast<DWORD*>(lLocked.pBits)==0xff000010u+luFace,"face write preserves all completed-history pixels");
        lpReadback->UnlockRect(); lpSource->Release(); lpSnapshot->Release();
    }
    Check(lHistory.End() && lHistory.Resolve(lpCube)==lpCube,"main-view sampler resumes after capture");
    Check(lHistory.Begin(lpDevice,lpCube,false),"first frame has a deterministic initialized snapshot");
    lHistory.End(); lpDevice->SetTexture(13,nullptr); lHistory.Release();
    Check(lHistory.GetSnapshot()==nullptr,"history releases default-pool resources for reset");

    D3DVIEWPORT9 lViewport={0,0,32,32,1,0};
    renderengine::DepthRangePC::SetViewport(lpDevice,lViewport);
    renderengine::DepthRangePC::SetDepthFunction(lpDevice,D3DCMP_GREATEREQUAL);
    DWORD luDepth=0; lpDevice->GetRenderState(D3DRS_ZFUNC,&luDepth);
    Check(luDepth==D3DCMP_LESSEQUAL,"native cube geometry has ordinary increasing depth");
    {
        ExtrasScope lScope;
        renderengine::DepthRangePC::SetDepthFunction(lpDevice,D3DCMP_LESSEQUAL);
        lpDevice->GetRenderState(D3DRS_ZFUNC,&luDepth);
        Check(luDepth==D3DCMP_LESSEQUAL,"glass and particles preserve geometry depth ordering");
        Check(ResolveExtrasDepthWrite(TRUE)==FALSE,"transparent captures never overwrite opaque depth");
        Check(ResolveExtrasCull(D3DCULL_CCW)==D3DCULL_CW && ResolveExtrasCull(D3DCULL_NONE)==D3DCULL_NONE,
            "mirrored face preserves authored one-sided and two-sided materials");
        suExtrasClipMask=1;
        Check(ResolveExtrasClipMask(0)==1 && ResolveExtrasClipMask(2)==3,
            "particle state binds preserve the independent distance clip plane");
        suExtrasClipMask=0;
    }
    Check(ResolveExtrasDepthWrite(TRUE)==TRUE && ResolveExtrasCull(D3DCULL_CCW)==D3DCULL_CCW,
        "main-view depth write and winding are restored");
    CgsPC::Shadows::suDepthFunction=D3DCMP_LESS;
    CgsPC::Shadows::sbDrawingDebris=true;
    Check(CgsPC::Shadows::ColourMask(15)==0 && CgsPC::Shadows::DepthWrite(FALSE)==TRUE
        && CgsPC::Shadows::DepthFunction(D3DCMP_ALWAYS)==D3DCMP_LESS,
        "debris uses cascade depth comparison and writes depth without colour");
    CgsPC::Shadows::sbDrawingDebris=false;
    Check(CgsPC::Shadows::ColourMask(15)==15 && CgsPC::Shadows::DepthWrite(FALSE)==FALSE,
        "debris shadow policy does not alter normal particle drawing");

    auto* lpLow=static_cast<u8*>(VirtualAlloc(reinterpret_cast<void*>(0x18000000),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!lpLow || reinterpret_cast<uintptr_t>(lpLow)>0xffffffffu)return 2;
    auto* lpMaterials=new(lpLow) CgsGraphics::MaterialTechnique[2]{};
    lpMaterials[1].mu16StateFlags=1;
    auto* lpTable=new(lpLow+256) CgsGraphics::Ptr32<CgsGraphics::MaterialTechnique>[2]{};
    lpTable[0].muSlot=static_cast<u32>(reinterpret_cast<uintptr_t>(&lpMaterials[0]));lpTable[1].muSlot=static_cast<u32>(reinterpret_cast<uintptr_t>(&lpMaterials[1]));
    CgsGraphics::MaterialAssembly laAssemblies[2]={};
    laAssemblies[0].mu8NumMaterials=laAssemblies[1].mu8NumMaterials=1;
    laAssemblies[0].mappMaterials.muSlot=static_cast<u32>(reinterpret_cast<uintptr_t>(lpTable));laAssemblies[1].mappMaterials.muSlot=static_cast<u32>(reinterpret_cast<uintptr_t>(lpTable+1));
    RenderableMesh laMeshes[2]={};
    laMeshes[0].mpMaterialAssembly=&laAssemblies[0];laMeshes[1].mpMaterialAssembly=&laAssemblies[1];
    laMeshes[0].mu8NumVertexDescriptors=laMeshes[1].mu8NumVertexDescriptors=1;
    RenderableMesh* lapMeshes[]={&laMeshes[0],&laMeshes[1]};
    Renderable lSource={};lSource.mu16NumMeshes=2;lSource.mppMeshes=lapMeshes;lSource.mBoundingSphere.w=9;
    alignas(16) CgsGraphics::DispatchCommand laBin[1024]={};
    CgsGraphics::DispatchFrame lFrame={};lFrame.GetBin().SetBinRange(laBin,laBin+1024);
    const auto* lpBody=FilterVehicleMeshes(&lSource,lFrame,7,false);
    const auto* lpGlass=FilterVehicleMeshes(&lSource,lFrame,7,true);
    Check(lpBody && lpBody->mu16NumMeshes==1 && lpBody->mppMeshes[0]==lapMeshes[0],"body pass excludes transparent windows");
    Check(lpGlass && lpGlass->mu16NumMeshes==1 && lpGlass->mppMeshes[0]==lapMeshes[1],"glass pass keeps authored transparent material");
    Check(lSource.mu16NumMeshes==2 && lSource.mppMeshes==lapMeshes,"capture leaves streamed renderable unchanged");
    Check(lpGlass->mBoundingSphere.w==9,"filtered glass retains model frustum bounds");
    Check(!FilterVehicleMeshes(lpBody,lFrame,0,true),"models without glass emit no transparent packet");
    VirtualFree(lpLow,0,MEM_RELEASE);
    lpReadback->Release();lpCube->Release();lpDevice->Release();lpApi->Release();DestroyWindow(lWindow);
    std::printf("PCReflectionCapture: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures?1:0;
}
