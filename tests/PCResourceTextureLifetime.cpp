// Reuse the native GPU pixel oracle; its standalone main is not invoked here.
#define main PCFlipResourcesUnusedMain
#include "PCFlipResources.cpp"
#undef main
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "pc/gcm/renderengine/TextureResourcePCLeaf.h"

static unsigned suHeapFrees, suAssertions;
static Texture* spExpectedClearedHeader;
static bool sbReleasedBeforeHeapFree;
static CgsResource::Entry* spReplacementEntry;
namespace CgsDev::Assert {
int BeginAssert(){return 0;}
int FireAssert(const char* p,const char*,int){++suAssertions;std::printf("ASSERT %s\n",p);return 0;}
void* EndAssert(){return nullptr;}
}
namespace renderengine {
void WorldGeometry_OnResourceMemoryFreed(const void*,size_t)
{
    if(spExpectedClearedHeader) sbReleasedBeforeHeapFree &= spExpectedClearedHeader->mpD3DTexture==nullptr;
}
}
namespace CgsResource {
// Heap storage is a fixture boundary. The production pool method must release
// its native object before reaching these frees, irrespective of byte storage.
void Heap::Free(u16){++suHeapFrees;}
void Heap::Free(void*){++suHeapFrees;}
Entry* Pool::FindResource(ID,bool,u16,s32* lpIndex)
{if(lpIndex)*lpIndex=0;return spReplacementEntry;}
}
#include "pc_resource_texture_lifetime.inc"

static Texture Header()
{
    Texture lHeader{};
    lHeader.miFormat=D3DFMT_A8R8G8B8;lHeader.muWidth=8;lHeader.muHeight=4;
    lHeader.muDepth=1;lHeader.muNumMipLevels=4;
    return lHeader;
}
static ULONG References(IDirect3DBaseTexture9* lpTexture)
{const ULONG lu=lpTexture->AddRef();lpTexture->Release();return lu-1;}
static void CheckMips(Sampler& lrSampler,Texture& lrTexture,unsigned luColour)
{
    bool lbPixels=lrTexture.mpD3DTexture!=nullptr;
    for(unsigned luMip=0;luMip<4;++luMip){const float laCoord[]={.5f,.5f,0,float(luMip)};
        lbPixels &= lrSampler.Pixel(lrTexture.mpD3DTexture,0,laCoord,Colour(luColour,luMip));}
    Check(lbPixels,"resource texture retains every rendered mip");
}
struct EntryFixture
{
    CgsResource::Pool mPool{};
    CgsResource::Entry mEntry{};
    CgsResource::RwRasterResourceType mType;
    Texture mHeader=Header();
    std::vector<DWORD> mPixels;
    explicit EntryFixture(unsigned luColour=0)
    {
        mPool.mbIsValid=true;mType.InitCachedValues();mEntry.mpResourceType=&mType;
        auto* lpHead=reinterpret_cast<CgsResource::BaseResourcePtr*>(&mEntry.mResource);
        mEntry.mpResourceNext=mEntry.mpResourcePrev=mEntry.mpResourceThis=lpHead;
        mEntry.muResourceThreadId=reinterpret_cast<uintptr_t>(&mEntry);
        for(unsigned luMip=0;luMip<4;++luMip)mPixels.insert(mPixels.end(),
            (8u>>luMip?8u>>luMip:1)*(4u>>luMip?4u>>luMip:1),Colour(luColour,luMip));
        mEntry.mResource.m_baseResources[0]=&mHeader;
        mEntry.mResource.m_baseResources[1]=mPixels.data();
        mEntry.mResource.m_baseResources[2]=nullptr;
        mEntry.mResourceDescriptor.m_baseResourceDescriptors[0].m_size=sizeof(mHeader);
        mEntry.mResourceDescriptor.m_baseResourceDescriptors[1].m_size=static_cast<u32>(mPixels.size()*sizeof(DWORD));
    }
    void Realize(){mPool.FixUpEntry(&mEntry);}
    void Free(){mPool.FreeMemoryForResource(&mEntry);}
};

static void TestLifetime(bool lbExtended)
{
    HWND lhWindow=CreateWindowA("STATIC","Resource texture lifetime",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
    IDirect3D9* lpApi=nullptr;IDirect3D9Ex* lpExApi=nullptr;IDirect3DDevice9* lpDevice=nullptr;IDirect3DDevice9Ex* lpExDevice=nullptr;
    D3DPRESENT_PARAMETERS lParams{};lParams.Windowed=TRUE;lParams.SwapEffect=lbExtended?D3DSWAPEFFECT_FLIPEX:D3DSWAPEFFECT_COPY;
    lParams.BackBufferCount=lbExtended?2:1;lParams.BackBufferWidth=lParams.BackBufferHeight=64;
    lParams.BackBufferFormat=D3DFMT_X8R8G8B8;lParams.hDeviceWindow=lhWindow;lParams.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT lhResult;
    if(lbExtended){lhResult=Direct3DCreate9Ex(D3D_SDK_VERSION,&lpExApi);lpApi=lpExApi;
        if(SUCCEEDED(lhResult))lhResult=lpExApi->CreateDeviceEx(0,D3DDEVTYPE_HAL,lhWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&lParams,nullptr,&lpExDevice);
        lpDevice=lpExDevice;
    }else{lpApi=Direct3DCreate9(D3D_SDK_VERSION);lhResult=lpApi?lpApi->CreateDevice(0,D3DDEVTYPE_HAL,lhWindow,D3DCREATE_HARDWARE_VERTEXPROCESSING,&lParams,&lpDevice):E_FAIL;}
    Check(SUCCEEDED(lhResult)&&lpDevice,"native lifetime-test device created");if(!lpDevice)return;
    renderengine::gDevice=lpDevice;
    {
        Sampler lSampler(lpDevice);
        EntryFixture lFirst;
        lFirst.Realize();CheckMips(lSampler,lFirst.mHeader,0);
        CgsResource::BaseResourcePtr lAlias;
        CgsResource::ResourceHandle lHandle;
        lHandle.mpResourceMemory=&lFirst.mEntry.mResource;lHandle.mpSourceEntry=&lFirst.mEntry;
        lAlias.CreateFromHandle(&lHandle);
        auto* lpRetained=lFirst.mHeader.mpD3DTexture;if(lpRetained)lpRetained->AddRef();
        spExpectedClearedHeader=&lFirst.mHeader;sbReleasedBeforeHeapFree=true;suHeapFrees=0;
        lFirst.Free();spExpectedClearedHeader=nullptr;
        Check(sbReleasedBeforeHeapFree&&suHeapFrees==2,"native release precedes both heap frees");
        Check(lpRetained&&References(lpRetained)==1,"real pool free releases exactly its native reference");
        Check(!lAlias.mpResourceMemory&&!lAlias.mHandle.mpResourceMemory&&!lAlias.mHandle.mpSourceEntry,
            "pool retirement publishes empty main and graphics pointers to retained aliases");
        // Also clean a deliberately omitted-retirement negative control.
        renderengine::TextureResource_OnEntryFreed(&lFirst.mEntry,nullptr);
        if(lpRetained)Check(lpRetained->Release()==0,"external native reference outlives resource bytes without a leak");

        EntryFixture lUnfixed;
        lUnfixed.mHeader.mpD3DTexture=reinterpret_cast<IDirect3DBaseTexture9*>(0x12340);
        lUnfixed.Free();
        Check(lUnfixed.mHeader.mpD3DTexture==reinterpret_cast<IDirect3DBaseTexture9*>(0x12340),
            "unfixed serialized bytes are never interpreted as COM ownership");

        EntryFixture lPartial(1);lPartial.Realize();
        auto* lpPartial=lPartial.mHeader.mpD3DTexture;if(lpPartial)lpPartial->AddRef();
        lPartial.Free();
        Check(lpPartial&&References(lpPartial)==1,"partially fixed resource frees before loaded-status publication");
        renderengine::TextureResource_OnEntryFreed(&lPartial.mEntry,nullptr);if(lpPartial)lpPartial->Release();

        EntryFixture lMoved(2);lMoved.Realize();
        auto* lpMoved=lMoved.mHeader.mpD3DTexture;if(lpMoved)lpMoved->AddRef();
        Texture lScratch=lMoved.mHeader,lDestination{};
        rw::Resource lSource{},lDest{};CgsResource::ResourceDescriptor lSize;
        lMoved.mEntry.mResource.ConvertToRWResource(lSource);lDest=lSource;lDest.m_baseResources[0]=&lScratch;
        lMoved.mType.ReBase(&lScratch,lSource,lDest,lSize,0);
        Check(lScratch.mpD3DTexture==lpMoved,"header move into scratch preserves native identity");
        CheckMips(lSampler,lScratch,2);
        lDestination=lScratch;lSource=lDest;lDest.m_baseResources[0]=&lDestination;
        lMoved.mType.ReBase(&lDestination,lSource,lDest,lSize,0);
        Check(lDestination.mpD3DTexture==lpMoved,"header move out of scratch preserves native identity");
        std::vector<DWORD> lMovedPixels=lMoved.mPixels;
        lSource=lDest;lDest.m_baseResources[2]=lMovedPixels.data();
        lMoved.mType.ReBase(&lDestination,lSource,lDest,lSize,2);
        std::memset(lMoved.mPixels.data(),0xCD,lMoved.mPixels.size()*sizeof(DWORD));
        Check(lDestination.mpD3DTexture==lpMoved,"pixel-block relocation does not recreate from address deltas");
        CheckMips(lSampler,lDestination,2);
        lMoved.mEntry.mResource.m_baseResources[0]=&lDestination;
        lMoved.Free();
        Check(lpMoved&&References(lpMoved)==1&&lDestination.mpD3DTexture==nullptr,"stable entry retires texture at its relocated header");
        renderengine::TextureResource_OnEntryFreed(&lMoved.mEntry,nullptr);if(lpMoved)lpMoved->Release();

        EntryFixture lA(3),lB(4);lA.Realize();lB.Realize();
        auto* lpA=lA.mHeader.mpD3DTexture;auto* lpB=lB.mHeader.mpD3DTexture;
        if(lpA)lpA->AddRef();if(lpB)lpB->AddRef();
        const Texture lTemp=lA.mHeader;lA.mHeader=lB.mHeader;lB.mHeader=lTemp;
        lA.mEntry.mResource.m_baseResources[0]=&lB.mHeader;
        lB.mEntry.mResource.m_baseResources[0]=&lA.mHeader;
        CheckMips(lSampler,lB.mHeader,3);CheckMips(lSampler,lA.mHeader,4);
        lA.Free();
        Check(lpA&&References(lpA)==1&&lpB&&References(lpB)==2,"reused header addresses cannot retire another entry's texture");
        lB.Free();Check(lpB&&References(lpB)==1,"second entry retains independent native ownership");
        renderengine::TextureResource_OnEntryFreed(&lA.mEntry,nullptr);
        renderengine::TextureResource_OnEntryFreed(&lB.mEntry,nullptr);
        if(lpA)lpA->Release();if(lpB)lpB->Release();

        EntryFixture lExplicit;lExplicit.Realize();
        auto* lpExplicit=lExplicit.mHeader.mpD3DTexture;if(lpExplicit)lpExplicit->AddRef();
        rw::Resource lResource{};lExplicit.mEntry.mResource.ConvertToRWResource(lResource);
        lExplicit.mType.FixDown(&lExplicit.mHeader,lResource);
        lExplicit.Free();
        Check(lpExplicit&&References(lpExplicit)==1,"explicit fix-down followed by pool free cannot double release");
        if(lpExplicit)lpExplicit->Release();

        EntryFixture lReplace(5);lReplace.Realize();
        auto* lpOld=lReplace.mHeader.mpD3DTexture;if(lpOld)lpOld->AddRef();
        renderengine::TextureResource_OnEntryFixedUp(&lReplace.mEntry,&lReplace.mHeader);
        Check(lpOld&&References(lpOld)==2,"repeated ownership publication does not add or release a reference");
        spReplacementEntry=&lReplace.mEntry;
        lReplace.mPool.DeleteMemoryForEntry(lReplace.mEntry.mID);
        Check(lpOld&&References(lpOld)==1,"live replacement releases the old native object before wiping resource bytes");
        renderengine::TextureResource_OnEntryFreed(&lReplace.mEntry,nullptr);if(lpOld)lpOld->Release();
        lReplace.mHeader=Header();
        for(unsigned luMip=0,luStart=0;luMip<4;++luMip){
            const unsigned luCount=(8u>>luMip?8u>>luMip:1)*(4u>>luMip?4u>>luMip:1);
            for(unsigned lu=0;lu<luCount;++lu)lReplace.mPixels[luStart++]=Colour(5,luMip);
        }
        lReplace.Realize();CheckMips(lSampler,lReplace.mHeader,5);
        auto* lpNew=lReplace.mHeader.mpD3DTexture;if(lpNew)lpNew->AddRef();
        lReplace.Free();
        Check(lpNew&&References(lpNew)==1,"a reused entry owns and retires the replacement independently");
        renderengine::TextureResource_OnEntryFreed(&lReplace.mEntry,nullptr);if(lpNew)lpNew->Release();
        spReplacementEntry=nullptr;
        Check(suAssertions==0,"lifetime paths raise no engine assertions");
    }
    renderengine::gDevice=nullptr;lpDevice->Release();lpApi->Release();DestroyWindow(lhWindow);
}
int main(){TestLifetime(false);TestLifetime(true);std::printf("PCResourceTextureLifetime: %d checks, %d failures\n",checks,failures);return failures?1:0;}
