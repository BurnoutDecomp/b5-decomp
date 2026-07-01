#pragma once

// ===================================================================================
// BrnGui::BankingScore -- element type home
//   b5-decomp/src/GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.h
//
// The per-score record the AboveCarRenderer banks up and renders above a crashed car.
// Declared here (the AboveCarRenderer class itself is owned by its own TU); this header
// supplies the BankingScore element type needed by the Array<BrnGui::BankingScore,6>
// container instantiation (CgsArrayBankingScore6.cpp) that AboveCarRenderer embeds as
// maBankingScores.
//
// Layout (DecFIGS DWARF BrnAboveCarRenderer.h:43; sizeof == 48, 16-aligned -- confirmed
// by the X360 Array<BankingScore,6> element stride of 48 bytes: index addressing
// `slwi r,1; add r,r; slwi r,4` == *48, count word at +0x120 == 6*48, and the 6-qword
// element copy in Append/EraseFast == 48 bytes):
//   +0x00  Vector2 mv2ScreenSpacePosition          (rw::math::vpu::Vector2, 16B, 16-aligned)
//   +0x10  Vector3 mv3OriginalWorldSpacePosition   (rw::math::vpu::Vector3, 16B)
//   +0x20  s16     miBaseScore
//   +0x22  s16     miComboBonus
//   +0x24  bool    mbIsRoadRageTimeExtension
//   +0x25..0x2F   trailing padding to the 16-byte struct alignment (sizeof 48)
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2 / Vector3 (rw::math::vpu) layout types
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"  // CgsGui::CustomRenderComponentInterface
#include "GameShared/GameClasses/Fonts/CgsFont.h"           // CgsResource::SafeResourceHandle<Font>
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"  // CgsGraphics::TextObject / TextRenderer / RGBA
#include "GameShared/GameClasses/Containers/CgsArray.h"      // Array<BankingScore, KI_MAX_BANKING_SCORES>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // CgsLanguage::LanguageManager (pointer member)
#include "GameSource/Replays/BrnGuiModuleAboveCarObjectLayout.h"  // BrnReplays::GuiModuleAboveCarObjectLayout (embedded array)
#include "GameSource/Replays/BrnReplayBaseSerialiser.h"       // BrnReplays::BaseSerialiser (mpGuiModuleSerialiser)
#include "GameSource/Replays/BrnReplayGuiModuleStaticLayout.h" // BrnReplays::GuiModuleStaticLayout (Update's transfer target)

namespace CgsGui { class ImRendererSet; }   // the active 2D/3D renderer set (matches BrnNetworkPlayerImageRenderer.h)
namespace renderengine { class BlendState; }
namespace rw { struct IResourceAllocator; }

namespace BrnGui
{
    // Forward-declared only: GuiCache is a large, still class:BrnGui::GuiCache-blocked type
    // (60 methods, 39 still un-homed). AboveCarRenderer only ever holds a pointer to it, so
    // the full header is intentionally NOT pulled in here (mirrors SatNavRenderer.h).
    class GuiCache;

    // KI_MAX_BANKING_SCORES (DWARF BrnAboveCarRenderer.h:35) -- the Array capacity (6).
    const s32 KI_MAX_BANKING_SCORES = 6;

    // KI_MAX_RECENTLY_HIT_CARS (DWARF BrnAboveCarRenderer.h:36) -- sizes mRecentCrashSet
    // (a BitArray<KI_MAX_RECENTLY_HIT_CARS...>-ish set; DWARF gives the raw bit count 601,
    // NOT this constant -- kept distinct per the DWARF-attested BitArray<601> template arg).
    const s32 KI_MAX_RECENTLY_HIT_CARS = 64;

    struct BankingScore
    {
        Vector2 mv2ScreenSpacePosition;        // +0x00
        Vector3 mv3OriginalWorldSpacePosition; // +0x10
        s16     miBaseScore;                   // +0x20
        s16     miComboBonus;                  // +0x22
        bool    mbIsRoadRageTimeExtension;     // +0x24
    };

    // KI_RIVALNAME_LENGTH / KI_POSITION_LENGTH (DWARF BrnAboveCarRenderer.h:55/56).
    const s32 KI_RIVALNAME_LENGTH = 64;
    const s32 KI_POSITION_LENGTH  = 16;

    // BrnGui::PlayerGamerTagAboveCarInfo (DWARF BrnAboveCarRenderer.h:53) -- one cached
    // above-car gamer-tag record (rival name + position text + colour + measured widths).
    // Only used by RenderComponent/RenderBankingScores (out of scope in this TU pass);
    // declared here by name so the class layout stays honest.
    struct PlayerGamerTagAboveCarInfo
    {
        bool  mbUsed;                              // +0x00
        char  macRivalName[KI_RIVALNAME_LENGTH];    // +0x?? (raw utf8 storage)
        const void* mpRivalName;                    // CgsUnicode::UnicodeBuffer::CgsUtf8* (opaque; uncommitted)
        char  macPositionText[KI_POSITION_LENGTH];
        const void* mpPositionText;                 // CgsUnicode::UnicodeBuffer::CgsUtf8* (opaque; uncommitted)
        CgsGraphics::RGBA mColour;
        f32   mfNameStringWidth;
        f32   mfPositionStringWidth;
        u32   muRivalPosition;
    };

    // BrnGui::AboveCarRenderer -- the GUI custom-render component that draws the marker /
    // score / gamer-tag / banking-score overlay above each car (DWARF BrnAboveCarRenderer.h:77;
    // AboveCarRenderer : public CgsGui::CustomRenderComponentInterface).
    //
    // Reconstructed from BURNOUT_X360_ARTIST.XEX against references/DecFIGS/dwarfdump/.../
    // BrnAboveCarRenderer.h. The ledger functions of this TU are reconstructed in
    // BrnAboveCarRenderer.cpp: Construct, GetID, Prepare, Release, Update (bodied);
    // RenderComponent / RenderBankingScores are declaration-only (see the .cpp for why --
    // both call BrnGui::GuiCache methods that are still [todo]/[blocked] on class:BrnGui::GuiCache,
    // plus RenderComponent additionally reaches the un-homed AboveCarRenderer::RenderReplayAboveCar
    // and SetTransformMatrixForCar helpers).
    //
    // LAYOUT POLICY (matches the sibling renderers SatNavRenderer / BoostBarRenderer /
    // CrashNavIconRenderer): the compile gate is a per-TU `cl /c` on a 64-bit host, so guest
    // byte offsets are NOT load-bearing (pointers widen 4->8). Members are declared BY NAME
    // from the DWARF; no raw offset casts are used anywhere in the .cpp. Member TYPES that are
    // still uncommitted (the resource-descriptor blocks, GuiCache, TextRenderer, LanguageManager)
    // are modelled as named opaque slots / forward-declared pointers so the in-scope bodies
    // type-check; their full layouts are intentionally OMITTED.
    class AboveCarRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // ---- DWARF nested enums (BrnAboveCarRenderer.h:79/85) ----
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 1,
        };

        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

    public:
        // ---- polymorphic component lifecycle (CgsGui::CustomRenderComponentInterface overrides) ----
        virtual void Construct();
        virtual bool Prepare(void* lpResourceAllocator, void* lpA, void* lpB);
        virtual bool Release();
        virtual void Update();
        // DWARF names this GetID() (CgsID, 64-bit content hash); the base interface's slot is
        // GetComponentID() (u32). Matches the SatNavRenderer precedent: the base virtual is
        // overridden under its declared name/signature, widening the low word out.
        virtual u32  GetComponentID() const;

        // ---- private render helpers (DWARF cpp:485/997; declaration-only -- see .cpp) ----
    private:
        virtual void RenderComponent(CgsGui::ImRendererSet* lpRendererSet);
        void RenderBankingScores(CgsGui::ImRendererSet* lpRendererSet);

        // ---- member state (DWARF h:186-217; declared by name, opaque where uncommitted) ----
    private:
        EPrepareStage mePrepareStage;                 // h:186
        EReleaseStage meReleaseStage;                  // h:187
        rw::IResourceAllocator* mpHeapAllocator;       // h:188

        PlayerGamerTagAboveCarInfo maCachedPlayerGameTagInfos[8]; // h:191 (E_ACTIVE_RACE_CAR_INDEX_COUNT == 8)

        // Per-active-race-car replay object layout cache, walked in lockstep with
        // maCachedPlayerGameTagInfos by Construct's zeroing loop (X360 @0x8245443C: a second
        // 32-byte-stride, 8-count array cleared via BrnReplays::GuiModuleAboveCarObjectLayout::
        // Clear() alongside the gamer-tag cache). Not named in the truncated DWARF dump for this
        // header; declared here BY NAME (not an offset cast) from the X360-attested stride/count/
        // callee. Only Construct (in scope) touches it.
        BrnReplays::GuiModuleAboveCarObjectLayout maAboveCarObjectLayouts[8];

        CgsResource::SafeResourceHandle<CgsResource::Font> mpScoreFont; // h:194

        Array<BankingScore, KI_MAX_BANKING_SCORES> maBankingScores;     // h:197

        CgsContainers::BitArray<601u> mRecentCrashSet;                  // h:201

        u32  mBlendStateResource[5];      // h:204 Resource (renderengine resource descriptor, opaque)
        renderengine::BlendState* mpBlendState; // h:205

        CgsGraphics::TextObject mTextObject;   // h:207
        CgsGraphics::TextRenderer* mpTextRenderer;  // h:208
        CgsLanguage::LanguageManager* mpLanguageManager; // h:209

        BrnGui::GuiCache* mpGuiCache;      // h:211

        s16  miTimeExtension;              // h:213
        bool mbTimeExtensionPending;       // h:214

        // Replay serialiser this component mirrors its cached above-car layout to/from
        // (X360 +1440; Update() reads its mode dword then GetStaticLayout()). Not in the
        // truncated DWARF dump for this header; the concrete leaf type
        // (BrnReplays::GuiModuleSerialiser) is a `.cpp`-local compile-only slice in
        // BrnReplayGuiModuleSerialiser.cpp with no shared header yet, so this is typed as
        // the real, fully-shared BrnReplays::BaseSerialiser (the common ancestor every
        // concrete serialiser derives from) rather than forking a duplicate declaration of
        // the leaf type locally. Update() reaches GetMode() + GetStaticBuffer() (both
        // BaseSerialiser's own public members) and reinterprets the static buffer as
        // GuiModuleStaticLayout, matching what GuiModuleSerialiser::GetStaticLayout() does
        // internally.
        BrnReplays::BaseSerialiser* mpGuiModuleSerialiser; // h:~1440 (X360, unattested in DWARF)

        s32  miAboveCarRendererPM;         // h:217 (perf-monitor handle)
    };
}
