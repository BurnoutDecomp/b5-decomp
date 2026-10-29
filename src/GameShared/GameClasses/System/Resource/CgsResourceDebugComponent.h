#ifndef CGS_RESOURCE_DEBUG_COMPONENT_H
#define CGS_RESOURCE_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"  // embedded mPoolTypeList
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolWindow.h"
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolHistogram.h"
#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"
#include "GameShared/GameClasses/System/Resource/CgsDebugBundleFilesWindow.h"

namespace CgsDev { namespace Log { struct LogFileBuffered; } }
namespace rw { struct IResourceAllocator; }

namespace CgsResource
{
class ResourceModule;
class Pool;

// The debug-component bring-up params carried in ResourceModule::InitOptions. The callback is the
// texture browser's draw hook, void(*)(renderengine::Texture*, Vector2, Vector2, void*, bool, bool);
// it is held as a plain pointer here so this header does not pull in the renderengine/Vector2
// headers. ConstructResourceModule fills: callback = TextureRenderCallback, userData = the
// GameDataModule, debugAllocator = Allocators::mpInternalDebugAllocator.
struct DebugComponentParams
{
    typedef CgsResource::DebugTextureRenderCallback DebugTextureRenderCallback;
    DebugTextureRenderCallback mpTextureRenderCallback;
    void*                   mpTextureRenderUserData;
    rw::IResourceAllocator* mpDebugAllocator;
};

// The resource subsystem's in-game debug component (path "Core", name "Resource"). ResourceModule
// embeds it by value, runs Construct from its own Construct and registers it with the debug manager
// at the end of its Prepare.
//
// The original window table and all five windows are embedded in DecFIGS
// declaration order, attested by ARTIST827E08D0 and the named consumers.
class DebugComponent : public CgsDev::DebugComponent
{
public:
    static const s32 KI_MAX_VISIBLE_POOLS = 4;

    DebugComponent();

    // Two-phase init: latch the owning module and the bring-up params, seed the type-list / sort /
    // render option tables and clear the dump and share-test flags.
    void Construct(ResourceModule* lpResourceModule, const DebugComponentParams* lpParams);
    void Update() ;

protected:
    void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay) ;
    void OnActivate() ;
    void InitWindows();
    void GenerateStringList();
    void UpdatePoolMenuItems();
    void UpdateWindows();
    void ResetWindow(s32 liWindow, const Pool* lpPool);
    void DumpStats();
    static void TriggerDefragCallback(void* lpData);
    static void IncTextureCallback(void* lpData);
    static void DecTextureCallback(void* lpData);
    static void IncPageCallback(void* lpData);
    static void DecPageCallback(void* lpData);
    static void TriggerStatsCallback(void* lpData);
    static void ShowBundleWindowChangeCallback(void* lpValue, void* lpData);

    virtual const char* GetName() const { return "Resource"; }
    virtual const char* GetPath() const;
    virtual bool        IsSimple() const { return false; }

    // One stats-dump row for lpPool: name, then main and video heap size, used and free, tab
    // separated.
    void DumpPoolStatistics(CgsDev::Log::LogFileBuffered& lrLogFile, const Pool* lpPool);

    // Total the main/video/local sizes of every resource loaded in share-test pool 0 that pool 1
    // also holds (looked up by id) into maiPoolShareTestResults.
    void UpdateShareTest();

    CgsDev::DebugUI::Window*     mapWindows[KI_MAX_VISIBLE_POOLS];
    DebugPoolWindow             mPoolWindow;
    DebugPoolHistogram          mPoolHistogram;
    DebugPoolTypeList            mPoolTypeList;
    DebugPoolTextures           mPoolTextures;
    DebugBundleFilesWindow      mBundleFilesWindow;
    s32                         maiPoolIds[KI_MAX_VISIBLE_POOLS];
    s32                         maiPrevPoolIds[KI_MAX_VISIBLE_POOLS];
    CgsDev::DebugUI::StringList maStringList[130];
    ResourceModule*             mpResourceModule;          // console +0x523C
    s32                         miTypeListMode;            // EPoolTypeListMode
    // ARTIST/DecFIGS has exactly three rows. The following sort-mode fields
    // supply the native list terminator: miSortMode@525C and first sort value@5260.
    CgsDev::DebugUI::StringList maTypeStringList[E_POOLTYPELISTMODE_COUNT];
    s32                         miSortMode;                // EDebugSortMode
    CgsDev::DebugUI::StringList maSortModeStringList[8];
    s32                         miRenderMode;              // EDebugTextureRenderMode
    CgsDev::DebugUI::StringList maRenderModeStringList[8];
    bool                        mbShowBundleWindow;
    bool                        mbTriggerStatsDump;
    DebugComponentParams        mParams;
    DebugComponentParams::DebugTextureRenderCallback mpTextureRenderCallback;
    s32                         miStatsDumpCount;
    s32                         miPoolShareTest0;          // console +0x52FC
    s32                         miPoolShareTest1;          // console +0x5300
    s32                         maiPoolShareTestResults[3];
    bool                        mbPoolShareTestVisible;
    bool                        mbShowBundleLoaderQueue;
private:
    static void AssertNativeLayout()
    {
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mPoolWindow) == 0x1c, "ARTIST resource pool window");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mPoolHistogram) == 0x3f8, "ARTIST resource histogram");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mPoolTypeList) == 0x480, "ARTIST resource type list");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mPoolTextures) == 0x4d94, "ARTIST resource texture window");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mBundleFilesWindow) == 0x4de8, "ARTIST bundle window");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mpResourceModule) == 0x523c, "ARTIST debug resource owner");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, maTypeStringList) == 0x5244, "ARTIST three type rows");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, miSortMode) == 0x525c, "ARTIST sort selector");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, maSortModeStringList) == 0x5260, "ARTIST sort rows");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, miRenderMode) == 0x52a0, "ARTIST render selector");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, maRenderModeStringList) == 0x52a4, "ARTIST render rows");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mParams) == 0x52e8, "ARTIST debug parameters");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, miStatsDumpCount) == 0x52f8, "ARTIST statistics counter");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mbPoolShareTestVisible) == 0x5310, "ARTIST share-test visibility");
        CGS_STATIC_ASSERT(offsetof(DebugComponent, mbShowBundleLoaderQueue) == 0x5311, "ARTIST loader queue visibility");
        CGS_STATIC_ASSERT(sizeof(DebugComponent) == 0x5314, "ARTIST resource debug span");
    }
};
}

#endif
