// CPU ownership oracle for the production cache. Pixel correctness is covered
// separately by PCFlipResources on real legacy and 9Ex devices.
#define NOMINMAX
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
#include "pc_texture_staging_cache.inc"

using namespace renderengine::TextureUploadPC;
static unsigned suChecks, suFailures;
static std::atomic<unsigned> suLive{0};
static void Check(bool lbPass, const char* lpcName)
{ ++suChecks; if (!lbPass) { ++suFailures; std::printf("FAIL %s\n", lpcName); } }

class TestTexture final : public IDirect3DBaseTexture9
{
    std::atomic<ULONG> muReferences{1};
public:
    TestTexture() { ++suLive; }
    ~TestTexture() { --suLive; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** lppOut) override
    { if (lppOut) *lppOut = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++muReferences; }
    ULONG STDMETHODCALLTYPE Release() override
    { const ULONG lu = --muReferences; if (!lu) delete this; return lu; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,const void*,DWORD,DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,void*,DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return E_NOTIMPL; }
    DWORD STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
    DWORD STDMETHODCALLTYPE GetPriority() override { return 0; }
    void STDMETHODCALLTYPE PreLoad() override {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_TEXTURE; }
    DWORD STDMETHODCALLTYPE SetLOD(DWORD) override { return 0; }
    DWORD STDMETHODCALLTYPE GetLOD() override { return 0; }
    DWORD STDMETHODCALLTYPE GetLevelCount() override { return 1; }
    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE) override { return E_NOTIMPL; }
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override { return D3DTEXF_NONE; }
    void STDMETHODCALLTYPE GenerateMipSubLevels() override {}
};

int main()
{
    StagingCache lCache;
    StagingKey lKey{reinterpret_cast<IDirect3DDevice9*>(1),D3DRTYPE_TEXTURE,D3DFMT_A8R8G8B8,8,4,1,4};
    auto* lpSource = new TestTexture;
    lCache.Retire(lpSource,lKey);
    Check(suLive == 1,"retirement transfers ownership without destroying reusable storage");
    StagingKey laOther[7] = {lKey,lKey,lKey,lKey,lKey,lKey,lKey};
    laOther[0].mpDevice = reinterpret_cast<IDirect3DDevice9*>(2);
    laOther[1].meType = D3DRTYPE_CUBETEXTURE;
    laOther[2].meFormat = D3DFMT_A8;
    laOther[3].muWidth = 16;
    laOther[4].muHeight = 8;
    laOther[5].muDepth = 6;
    laOther[6].muLevels = 3;
    for (const auto& lrOther : laOther)
    {
        auto* lpWrong = lCache.Take(lrOther);
        Check(lpWrong == nullptr,"device/type/format/dimensions/mip count isolate cache entries");
        if (lpWrong) lpWrong->Release();
    }
    auto* lpTaken = lCache.Take(lKey);
    Check(lpTaken == lpSource,"matching request receives the original staging object");
    Check(lCache.Take(lKey) == nullptr,"taken storage cannot serve two live textures");
    if (lpTaken) Check(lpTaken->Release() == 0,"taking transfers exactly one owned reference");
    Check(suLive == 0,"cache hits leave no hidden owner");

    for (unsigned lu = 0; lu < 129; ++lu)
    {
        auto lUnique = lKey; lUnique.muWidth += lu;
        lCache.Retire(new TestTexture,lUnique);
    }
    Check(suLive == 128,"object count remains bounded across different texture shapes");
    Check(lCache.Take(lKey) == nullptr,"oldest retired object is evicted when full");
    lCache.Clear();
    Check(suLive == 0,"explicit purge releases every cached object");

    auto lLarge = lKey; lLarge.muWidth = 1024; lLarge.muHeight = 2048; lLarge.muLevels = 1;
    for (unsigned lu = 0; lu < 3; ++lu) lCache.Retire(new TestTexture,lLarge);
    Check(suLive == 2,"padded storage budget also bounds large textures");
    lCache.Clear();
    lLarge.muHeight = 8192;
    lCache.Retire(new TestTexture,lLarge);
    Check(suLive == 0,"oversized texture is released instead of exceeding the cache budget");
    lLarge.muWidth = lLarge.muHeight = lLarge.muDepth = ~UINT(0);
    Check(StagingCache::StorageBytes(lLarge) == 0,"extreme dimensions cannot overflow budget arithmetic");
    auto lUnknown = lKey; lUnknown.meFormat = D3DFMT_UNKNOWN;
    lCache.Retire(new TestTexture,lUnknown);
    Check(suLive == 0,"unknown storage format follows ordinary release");
    auto lInvalid = lKey; lInvalid.muLevels = 0;
    lCache.Retire(new TestTexture,lInvalid);
    Check(suLive == 0,"unresolved mip count cannot enter the cache");

    {
        StagingCache lTemporary;
        lTemporary.Retire(new TestTexture,lKey);
    }
    Check(suLive == 0,"cache destruction releases retained COM references");
    auto lWorker = [&] {
        for (unsigned lu = 0; lu < 1000; ++lu)
        {
            auto* lp = lCache.Take(lKey);
            if (!lp) lp = new TestTexture;
            lCache.Retire(lp,lKey);
        }
    };
    std::thread lFirst(lWorker), lSecond(lWorker);
    lFirst.join(); lSecond.join(); lCache.Clear();
    Check(suLive == 0,"concurrent ownership transfers neither duplicate nor leak storage");
    std::printf("PCTextureStagingCache: %u checks, %u failures\n",suChecks,suFailures);
    return suFailures ? 1 : 0;
}
