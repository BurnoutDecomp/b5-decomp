#include "GameShared/GameClasses/Development/DebugSystem/CgsDebugFontBringUp.h"

#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"             // Pool, InitOptions, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"     // BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"     // ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"          // E_RESOURCETYPE_FONT
#include "GameShared/GameClasses/Fonts/CgsFont.h"                               // Font, SafeResourceHandle
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h" // DebugManager::SetDebugFont

#include <cstdlib>   // malloc

namespace CgsDev
{
    namespace
    {
        // A small dedicated pool for the debug font + its atlas rasters. The pool bump-allocates its
        // overhead + heaps from this backing memory (over-reserving is safe); generous fixed sizes so
        // a debug-font atlas comfortably fits without knowing the bundle's exact descriptor up front.
        const u32 KU_POOL_BYTES   = 4u * 1024u * 1024u;   // per memory pool
        const u32 KU_MAIN_HEAP    = 3u * 1024u * 1024u;   // main pool also holds the entry/hash overhead
        const u32 KU_GFX_HEAP     = KU_POOL_BYTES - 64u * 1024u;
        const u32 KU_MAX_RESOURCES = 64u;
        const u32 KU_MAX_NODES     = 256u;

        bool glbDebugFontLoaded = false;
    }

    bool LoadAndSetDebugFont(const char* lpcBundlePath, DebugManager& lrManager)
    {
        if (glbDebugFontLoaded)
            return true;   // already brought up

        // 1. register the reconstructed resource-type handlers (font + the rasters it imports).
        CgsResource::RegisterAllResourceTypes();

        // 2. stand up the pool over malloc'd backing memory.
        void* lpMainMem  = malloc(KU_POOL_BYTES);
        void* lpGfxSysMem = malloc(KU_POOL_BYTES);
        void* lpGfxLclMem = malloc(KU_POOL_BYTES);
        if (lpMainMem == 0 || lpGfxSysMem == 0 || lpGfxLclMem == 0)
        {
            free(lpMainMem); free(lpGfxSysMem); free(lpGfxLclMem);
            return false;
        }

        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId    = 0;
        lOptions.mpcName = "DebugFont";

        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muMaxNodes       = KU_MAX_NODES;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muHeapMemorySize = KU_MAIN_HEAP;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muHeapAlignment  = 16u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muMaxNodes       = KU_MAX_NODES;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muHeapMemorySize = KU_GFX_HEAP;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muHeapAlignment  = 16u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muMaxNodes       = KU_MAX_NODES;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muHeapMemorySize = KU_GFX_HEAP;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muHeapAlignment  = 16u;

        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]      = lpMainMem;
        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM] = lpGfxSysMem;
        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL]  = lpGfxLclMem;

        for (u32 luMemType = 0; luMemType < CgsResource::E_MEMTYPE_NUMTYPES; ++luMemType)
        {
            lOptions.mDescriptor.m_baseResourceDescriptors[luMemType].m_size      = KU_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[luMemType].m_alignment = 16u;
        }

        lOptions.muMaxResources        = KU_MAX_RESOURCES;
        lOptions.muMaxImports          = KU_MAX_RESOURCES;
        lOptions.miRefCountThreshold   = 0;
        lOptions.miNumDependencies     = 0;
        lOptions.miBankId              = 0;
        lOptions.mbAllowDefragmentation = false;

        static CgsResource::Pool sFontPool;
        sFontPool.Construct();
        sFontPool.InitPool(&lOptions);

        // 3. load the bundle (PC synchronous loader: read -> create -> copy -> fixup -> resolve imports).
        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lpcBundlePath, &sFontPool, CgsResource::ResolveResourceType);
        if (liLoaded <= 0)
            return false;   // bundle missing / unreadable -> keep the vector font

        // 4. pick the Font out of the loaded set.
        s32 liIndex = -1;
        CgsResource::Entry* lpEntry = sFontPool.FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liIndex);
        if (lpEntry == 0)
            return false;
        CgsResource::Font* lpFont =
            reinterpret_cast<CgsResource::Font*>(lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]);
        if (lpFont == 0)
            return false;

        // 5. build the runtime texture state (binds atlas page 0) so the text renderer can draw with it.
        lpFont->CreateTextureState();

        // 6. hand the font to the debug renderers (flips DrawText onto the bitmap path).
        CgsResource::SafeResourceHandle<CgsResource::Font> lHandle;
        lHandle.mpResource = lpFont;
        lHandle.muSafetyId = 1u;   // non-null: HasResourceFont() now true
        lrManager.SetDebugFont(lHandle);

        glbDebugFontLoaded = true;
        return true;
    }
}
