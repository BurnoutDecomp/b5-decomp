#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"   // TypeRegistry::Register

#include "GameShared/GameClasses/RenderWare/CgsRwRasterResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsRwTextureStateResourceType.h"
#include "GameShared/GameClasses/RenderWare/CgsMaterialStateResourceType.h"
#include "GameShared/GameClasses/Fonts/Resources/CgsFontResourceType.h"
#include "GameShared/GameClasses/Graphics/Resources/CgsVideoDataResource.h"
#include "GameShared/GameClasses/RenderWare/cross/CgsModelResourceType.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstanceListResourceType.h"
#include "GameShared/GameClasses/System/Resource/CgsEntryListResource.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListResourceType.h"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceHierarchyResourceType.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsSnrResourceType.h"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysSchemaResourceType.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsAptDataHeaderType.h"
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"   // CgsResource::LuaCodeResourceType (0x22)
#include "GameShared/GameClasses/Language/Resources/CgsLanguageResourceType.h" // CgsResource::LanguageResourceType (0x27)
#include "SharedClasses/Gui/Flapt/BrnFlaptFileResourceType.h"                  // BrnFlapt::FlaptFileResourceType (0x10020)

// ============================================================================================
// Resource-type registration -- the faithful counterpart of the X360
// BrnResource::GameDataModule::RegisterResourceTypes (0x82667EA8).
//
// HOW THE GAME DOES IT (0x82667EA8). The X360 registers its resource-type handlers in ONE
// monolithic GameDataModule method: for each of ~75 types it heap-allocates a CgsResource::Type
// (operator new), stamps the handler's vtable, calls Type::InitCachedValues, and stores the
// handler pointer + its name string into two parallel arrays owned by the GameDataModule
// (a handler table at this+0x60E8C-ish and a name table at this+0x60E0C), bumping a running
// counter. Resolution then indexes those module arrays.
//
// HOW WE DO IT, AND WHY IT DEVIATES. This stand-in instead installs each handler as a
// function-local static into the global CgsResource::TypeRegistry (the table BundleLoader's
// ResolveResourceType walks). Three deliberate, documented deviations:
//   1. *Location*: the real function is a game-layer BrnResource::GameDataModule method. This
//      lives engine-side (CgsResource) because the early boot path brings resources online
//      BEFORE GameDataModule::Prepare runs -- the debug-font bring-up and the GUI MovieManager
//      both LoadBundle (Default.font / VIDEOLIST.BUNDLE) during Construct, so they need the
//      handlers registered without a prepared game-layer module. Routing that through a
//      game-layer method would invert the engine->game layering.
//   2. *Storage*: global TypeRegistry vs. the GameDataModule's own handler/name arrays. The
//      arrays + their resolution path are a later reconstruction; until then BundleLoader
//      resolves through TypeRegistry, so we register there.
//   3. *Mechanism*: static singletons vs. heap-new'd Type objects + InitCachedValues. Static
//      instances are boot-safe and need no allocator; the cached-value init is folded into each
//      handler's own ctor where it matters.
//
// WHY IT IS ONE FUNCTION (and was not). This was previously split into a per-handler
// Register<Name>ResourceType() helper in each handler .cpp, gathered here by name. That spread
// registration across a dozen files and did NOT mirror the game, whose registration is a single
// function. The helpers have been removed and their bodies inlined below, in the GAME'S OWN
// REGISTRATION ORDER, so this reads as the one monolith the original source was -- minus the
// three deviations above. The handler .cpp files now only define behaviour, not registration.
//
// INCREMENTAL. The game registers ~75 types; only the handlers reconstructed so far are listed.
// The rest are added here, in their game-order slot, as each handler TU lands. Two of the
// entries below (EntryList, AptDataHeader) are NOT in GameDataModule::RegisterResourceTypes at
// all -- the X360 registers those in their owning module's bring-up (GUI / resource subsystem),
// not the game-data module. They are folded in here as a temporary bridge until those modules'
// registration is reconstructed; see the note at their call sites.
// ============================================================================================

namespace CgsResource
{
    void RegisterAllResourceTypes()
    {
        // Idempotent: the debug-font bring-up, the GUI module's movie-bundle load, and the
        // GameDataModule prepare all reach this; register the handler set exactly once.
        static bool sbRegistered = false;
        if (sbRegistered)
            return;
        sbRegistered = true;

        // ---- in BrnResource::GameDataModule::RegisterResourceTypes order (0x82667EA8) --------
        static RwRasterResourceType        sRwRaster;          // [game #1]  0x00  Texture / RwRaster
        TypeRegistry::Register(&sRwRaster);
        static RwTextureStateResourceType  sRwTextureState;    // [game #5]  0x0E  TextureState
        TypeRegistry::Register(&sRwTextureState);
        static MaterialStateResourceType   sMaterialState;     // [game #6]  0x0F  MaterialState / BlendState
        TypeRegistry::Register(&sMaterialState);
        static FontResourceType            sFont;              // [game #16] 0x21  Font (imports rasters)
        TypeRegistry::Register(&sFont);
        static VoiceHierarchyResourceType  sVoiceHierarchy;    // [game #25] 0x18  VoiceHierarchy (sound)
        TypeRegistry::Register(&sVoiceHierarchy);
        static SnrResourceType             sSnr;               // [game #27] 0x19  SNR (streamed audio)
        TypeRegistry::Register(&sSnr);
        static VideoDataResourceType       sVideoData;         // [game #30] 0x42  VideoData (movie metadata)
        TypeRegistry::Register(&sVideoData);
        static CgsGraphics::InstanceListResourceType sInstanceList; // [game #32] 0x23  InstanceList (world instances)
        TypeRegistry::Register(&sInstanceList);
        static AttribSysSchemaResourceType sAttribSysSchema;   // [game #36] 0x1B  AttribSys schema
        TypeRegistry::Register(&sAttribSysSchema);
        static ModelResourceType           sModel;             // [game #38] 0x2A  Model (serialised model)
        TypeRegistry::Register(&sModel);
        static PolygonSoupListResourceType sPolygonSoupList;   // [game #71] 0x43  PolygonSoupList (collision)
        TypeRegistry::Register(&sPolygonSoupList);

        // ---- owning-module registrations (NOT in GameDataModule::RegisterResourceTypes) -------
        // The X360 registers these in their owning subsystem's bring-up, not the game-data module.
        // Folded in here as a temporary bridge until that registration is reconstructed.
        static EntryListResourceType       sEntryList;         // 0x1D  EntryList (resource subsystem)
        TypeRegistry::Register(&sEntryList);
        static AptDataHeaderType           sAptDataHeader;     // 0x1E  AptData (GUI/Flash movie header)
        TypeRegistry::Register(&sAptDataHeader);
        static LuaCodeResourceType         sLuaCode;           // 0x22  LuaCode (FSM scripts; loaded by the GUI flow)
        TypeRegistry::Register(&sLuaCode);
        static LanguageResourceType        sLanguage;          // 0x27  Language (localised string table)
        TypeRegistry::Register(&sLanguage);
        static BrnFlapt::FlaptFileResourceType sFlaptFile;     // 0x10020 FLApt (GUI-owned vendor movie)
        TypeRegistry::Register(&sFlaptFile);
    }
}
