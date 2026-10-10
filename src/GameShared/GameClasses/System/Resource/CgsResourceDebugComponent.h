#ifndef CGS_RESOURCE_DEBUG_COMPONENT_H
#define CGS_RESOURCE_DEBUG_COMPONENT_H

#include "DebugSystem/Core/CgsDebugComponent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceDebugPoolTypeList.h"  // embedded mPoolTypeList

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
    void*                   mpTextureRenderCallback;
    void*                   mpTextureRenderUserData;
    rw::IResourceAllocator* mpDebugAllocator;
};

// The resource subsystem's in-game debug component (path "Core", name "Resource"). ResourceModule
// embeds it by value, runs Construct from its own Construct and registers it with the debug manager
// at the end of its Prepare.
//
// Layout follows the class's declared member order. The console also embeds a window table
// (mapWindows[4]) and four sub-windows ahead of maiPoolIds -- the pool window, the pool histogram,
// the texture browser (DebugPoolTextures) and the bundle-files window -- beside mPoolTypeList; those
// five members are not part of this class in this tree yet.
class DebugComponent : public CgsDev::DebugComponent
{
public:
    static const s32 KI_MAX_VISIBLE_POOLS = 4;

    DebugComponent();

    // Two-phase init: latch the owning module and the bring-up params, seed the type-list / sort /
    // render option tables and clear the dump and share-test flags.
    void Construct(ResourceModule* lpResourceModule, const DebugComponentParams* lpParams);

protected:
    virtual const char* GetName() const { return "Resource"; }
    virtual const char* GetPath() const;
    virtual bool        IsSimple() const { return false; }

    // One stats-dump row for lpPool: name, then main and video heap size, used and free, tab
    // separated.
    void DumpPoolStatistics(CgsDev::Log::LogFileBuffered& lrLogFile, const Pool* lpPool);

    // Total the main/video/local sizes of every resource loaded in share-test pool 0 that pool 1
    // also holds (looked up by id) into maiPoolShareTestResults.
    void UpdateShareTest();

    DebugPoolTypeList           mPoolTypeList;
    s32                         maiPoolIds[KI_MAX_VISIBLE_POOLS];
    s32                         maiPrevPoolIds[KI_MAX_VISIBLE_POOLS];
    CgsDev::DebugUI::StringList maStringList[130];
    ResourceModule*             mpResourceModule;          // console +0x523C
    s32                         miTypeListMode;            // EPoolTypeListMode
    CgsDev::DebugUI::StringList maTypeStringList[3];
    s32                         miSortMode;                // EDebugSortMode
    CgsDev::DebugUI::StringList maSortModeStringList[8];
    s32                         miRenderMode;              // EDebugTextureRenderMode
    CgsDev::DebugUI::StringList maRenderModeStringList[8];
    bool                        mbShowBundleWindow;
    bool                        mbTriggerStatsDump;
    DebugComponentParams        mParams;
    void*                       mpTextureRenderCallback;
    s32                         miStatsDumpCount;
    s32                         miPoolShareTest0;          // console +0x52FC
    s32                         miPoolShareTest1;          // console +0x5300
    s32                         maiPoolShareTestResults[3];
    bool                        mbPoolShareTestVisible;
    bool                        mbShowBundleLoaderQueue;
};
}

#endif
