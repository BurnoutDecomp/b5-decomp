#include "GameShared/GameClasses/Development/DebugSystem/CgsDebugFontBringUp.h"

#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"             // Pool, InitOptions, Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"     // BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"     // ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"          // E_RESOURCETYPE_FONT
#include "GameShared/GameClasses/Fonts/CgsFont.h"                               // Font, SafeResourceHandle
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h" // DebugManager::SetDebugFont
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // WriteToLog (diagnostics)
#include "pc/gcm/renderengine/texture.h"                                        // renderengine::Texture (atlas dump)
#include "pc/gcm/renderengine/device.h"                                         // renderengine::gDevice (defer until up)

#include <cstdlib>   // malloc
#include <cstdio>    // snprintf (diagnostics)

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

        // The atlas raster's FixUp creates a D3D texture, which needs the device. The boot path runs
        // this from Construct BEFORE the device exists (gDevice still null -> Texture::Create silently
        // no-ops, atlas comes back d3d=NULL). So bail WITHOUT latching until the device is up; the
        // render path (DispatchThread) retries every frame, and the first call after the device exists
        // does the real load + creates the atlas texture. [boot ordering]
        if (renderengine::gDevice == nullptr)
            return false;

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

        // sFontPool is static (zero-initialized); InitPool stands up everything it needs, so the
        // separate Pool::Construct() lifecycle call (deferred) is not required here.
        static CgsResource::Pool sFontPool;
        sFontPool.InitPool(&lOptions);

        // 3. load the bundle (PC synchronous loader: read -> create -> copy -> fixup -> resolve imports).
        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lpcBundlePath, &sFontPool, CgsResource::ResolveResourceType);
        CgsDev::Log::WriteToLog("[DebugFont] LoadBundle(\"");
        CgsDev::Log::WriteToLog(lpcBundlePath);
        CgsDev::Log::WriteToLog(liLoaded <= 0
            ? "\") FAILED (missing path, or not an uncompressed platform-4 v2 bundle) -> vector font.\n"
            : "\") ok.\n");
        if (liLoaded <= 0)
            return false;   // bundle missing / unreadable -> keep the vector font

        // 4. pick the Font out of the loaded set.
        s32 liIndex = -1;
        CgsResource::Entry* lpEntry = sFontPool.FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liIndex);
        if (lpEntry == 0)
        {
            CgsDev::Log::WriteToLog("[DebugFont] no Font (type 0x21) resource in the bundle -> vector font.\n");
            return false;
        }
        CgsResource::Font* lpFont =
            reinterpret_cast<CgsResource::Font*>(lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]);
        if (lpFont == 0)
            return false;

        // 5. build the runtime texture state (binds atlas page 0) so the text renderer can draw with it.
        lpFont->CreateTextureState();

        // Diagnostics: dump the loaded font's metrics + atlas so a bad conversion is visible in the log.
        {
            char lac[256];
            std::snprintf(lac, sizeof(lac),
                "[DebugFont] ver=%u size=%u numChars=%u pages=%u scaleUV=(%.5f,%.5f) heightPx=%u\n",
                lpFont->muVersionId, lpFont->mSizeOfFont, lpFont->muNumChars, lpFont->muNumTexturePages,
                static_cast<double>(lpFont->mScaleUV.mX), static_cast<double>(lpFont->mScaleUV.mY),
                lpFont->muFontHeightInPixels);
            CgsDev::Log::WriteToLog(lac);

            renderengine::Texture* lpAtlas =
                (lpFont->mpapTextures != 0 && lpFont->muNumTexturePages > 0) ? lpFont->mpapTextures[0] : 0;
            if (lpAtlas != 0)
            {
                std::snprintf(lac, sizeof(lac), "[DebugFont] atlas0 fmt=%d %ux%u mips=%u d3d=%s\n",
                    lpAtlas->miFormat, lpAtlas->muWidth, lpAtlas->muHeight, lpAtlas->muNumMipLevels,
                    lpAtlas->mpD3DTexture ? "created" : "NULL");
                CgsDev::Log::WriteToLog(lac);
            }
            else
            {
                CgsDev::Log::WriteToLog("[DebugFont] atlas0 <none>\n");
            }

            if (lpFont->muNumChars > 0 && lpFont->mpaFontChars != 0 && lpFont->mpaFontCharIds != 0)
            {
                const CgsResource::FontChar& lrFc = lpFont->mpaFontChars[0];
                std::snprintf(lac, sizeof(lac),
                    "[DebugFont] glyph0 id=%u TL=(%.4f,%.4f) dim=(%.4f,%.4f) start=(%.4f,%.4f) adv=%.4f page=%u rend=%u\n",
                    lpFont->mpaFontCharIds[0],
                    static_cast<double>(lrFc.mTopLeftUV.mX), static_cast<double>(lrFc.mTopLeftUV.mY),
                    static_cast<double>(lrFc.mDimensionsUV.mX), static_cast<double>(lrFc.mDimensionsUV.mY),
                    static_cast<double>(lrFc.mStart.mX), static_cast<double>(lrFc.mStart.mY),
                    static_cast<double>(lrFc.mfAdvance), lrFc.mu16TexturePageId, lrFc.mbIsRenderable);
                CgsDev::Log::WriteToLog(lac);
            }
        }

        // 6. hand the font to the debug renderers (flips DrawText onto the bitmap path). Build a real
        // canonical SafeResourceHandle: mpResourceMemory points at the pool entry's main-memory resource
        // slot (entry.mResource[0], which holds the Font*), so the handle's double-deref yields the Font --
        // the faithful X360 model (mpResourceMemory -> SmallResource -> resource ptr).
        CgsResource::SafeResourceHandle<CgsResource::Font> lHandle;
        lHandle.mpResourceMemory = &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        lHandle.mpSourceEntry    = lpEntry;
        lrManager.SetDebugFont(lHandle);

        glbDebugFontLoaded = true;
        CgsDev::Log::WriteToLog("[DebugFont] bitmap font loaded + set; DrawText now uses the resource font.\n");
        return true;
    }
}
