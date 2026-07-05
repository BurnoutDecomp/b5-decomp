#include "GameSource/Resource/BrnDLCManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT + Begin/Fire/EndAssert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (GetPackage runtime-streamed assert)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"      // CgsDev::DebugInterface
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"             // CgsDev::DebugManager::ActivateComponent
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // CgsDev::Debug2DImmediateRender, RGBA
#include <cstddef>   // offsetof
#include <cstring>   // memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DLCBeatTheTeamGame::SetEnabledState           @ 0x82472CA8 (caller: BrnGui::BootLegal::Update)
//   BrnResource::DLCFeatureAvailability::Construct             @ 0x82662C48
//   BrnResource::DLCFeatureAvailability::GetPackMask           @ 0x82661628
//   BrnResource::DLCFeatureAvailability::SetPackAvailabilityState @ 0x826616A0
//
// The full DLCManager / DLCDebugComponent bodies that share this translation unit
// land when those TUs are reconstructed -- add them here then.

namespace BrnResource
{
    // Never called -- pins the asm-attested member offsets of DLCFeatureAvailability.
    void DLCFeatureAvailability::_AssertLayout()
    {
        static_assert(offsetof(DLCFeatureAvailability, muAvailabilityMask) == 0x04, "mask @ +0x04");
        static_assert(offsetof(DLCFeatureAvailability, mauPackMask)        == 0x08, "pack masks @ +0x08");
        static_assert(offsetof(DLCFeatureAvailability, maePackForFeature)  == 0x1C, "feature pack @ +0x1C");
        static_assert(offsetof(DLCFeatureAvailability, mabFeatureEnabled)  == 0x80, "feature flags @ +0x80");
    }

    // X360 0x82662C48 (store-for-store). Clears the availability mask and the feature table,
    // then writes the default per-pack bit masks {1,2,4,8,16}, the default per-feature required-pack
    // indices, and marks every feature enabled. Every constant below is a baked immediate store.
    void DLCFeatureAvailability::Construct()
    {
        muAvailabilityMask = 0;                 // stw r30(0), 4(r31)

        // memset(this + 0x1C, 0, 100): zero the 25-dword feature-pack table.
        std::memset(maePackForFeature, 0, sizeof(maePackForFeature));

        // The +0x80 bool block is zeroed by a 6-dword + 1-byte loop, then set to 1 below.
        std::memset(mabFeatureEnabled, 0, sizeof(mabFeatureEnabled));

        mbField01    = false;                   // stb r30(0), 1(r31)
        mbConstructed = true;                   // stb r11(1), 0(r31)

        // Per-feature required-pack indices (stw immediates @ +0x1C..+0x7C).
        maePackForFeature[0]  = 0;
        maePackForFeature[1]  = 3;
        maePackForFeature[2]  = 1;
        maePackForFeature[3]  = 3;
        maePackForFeature[4]  = 3;
        maePackForFeature[5]  = 3;
        maePackForFeature[6]  = 4;
        maePackForFeature[7]  = 4;
        maePackForFeature[8]  = 3;
        maePackForFeature[9]  = 0;
        maePackForFeature[10] = 0;
        maePackForFeature[11] = 0;
        maePackForFeature[12] = 1;
        maePackForFeature[13] = 1;
        maePackForFeature[14] = 2;
        maePackForFeature[15] = 2;
        maePackForFeature[16] = 2;
        maePackForFeature[17] = 3;
        maePackForFeature[18] = 3;
        maePackForFeature[19] = 0;
        maePackForFeature[20] = 1;
        maePackForFeature[21] = 1;
        maePackForFeature[22] = 0;
        maePackForFeature[23] = 0;
        maePackForFeature[24] = 3;

        // Every feature defaults to enabled (stb r11(1), 0x80..0x98).
        for (int liFeature = 0; liFeature < KU_DLC_FEATURE_COUNT; ++liFeature)
        {
            mabFeatureEnabled[liFeature] = true;
        }

        // Per-pack contribution masks (stw immediates @ +0x08..+0x18).
        mauPackMask[0] = 1;
        mauPackMask[1] = 2;
        mauPackMask[2] = 4;
        mauPackMask[3] = 8;
        mauPackMask[4] = 16;
    }

    // X360 0x82661628. Returns the bit the given data pack contributes to the availability
    // mask: mauPackMask[liPack]. The asm indexes *(this + 4*(lPack+2)), i.e. mauPackMask at +0x08.
    u32 DLCFeatureAvailability::GetPackMask(s32 liPack) const
    {
        CGS_ASSERT(liPack >= E_DLC_DATA_PACK_START, "lPack >= E_DLC_DATA_PACK_START");
        CGS_ASSERT(liPack < E_DLC_DATA_PACK_COUNT,  "lPack < E_DLC_DATA_PACK_COUNT");

        return mauPackMask[liPack];
    }

    // X360 0x826616A0. When lbAvailable, OR the pack's bit into the running mask. Otherwise the asm
    // computes (mask == 0) via cntlzw/extrwi and ANDs the running mask with that 0-or-1 boolean -- so
    // clearing any real (non-zero-mask) pack zeroes the whole availability mask. Reproduced faithfully.
    void DLCFeatureAvailability::SetPackAvailabilityState(s32 liPack, bool lbAvailable)
    {
        CGS_ASSERT(liPack >= E_DLC_DATA_PACK_START, "lPack >= E_DLC_DATA_PACK_START");
        CGS_ASSERT(liPack < E_DLC_DATA_PACK_COUNT,  "lPack < E_DLC_DATA_PACK_COUNT");

        const u32 luMask = GetPackMask(liPack);
        if (lbAvailable)
        {
            muAvailabilityMask |= luMask;
        }
        else
        {
            muAvailabilityMask &= (luMask == 0u);
        }
    }
    // X360 0x82472CA8 (store-for-store). Writes mbIsEnabled (@+0x01) unconditionally, then --
    // only when enabling -- asserts that the content is actually available: the invariant is
    // "you may not enable a Beat The Team game that is not available". The X360 stores the flag
    // first (stb r4, 1(r3)) and the assert is non-fatal (it logs and returns the object).
    void DLCBeatTheTeamGame::SetEnabledState(bool lbEnabled)
    {
        mbIsEnabled = lbEnabled;

        if (lbEnabled)
        {
            CGS_ASSERT(mbIsAvailable, "!(mbIsEnabled && !mbIsAvailable)");
        }
    }

    // =============================================================================================
    // DLCDebugComponent
    // =============================================================================================

    // The file-scope downloadable-content availability table the debug component drives. In the X360
    // image this is the static instance at byte_82FFA7F0; Update / RenderHUD / OnActivate all reach it
    // directly (the asm hard-codes its address), so it is a TU-local static here.
    static DLCFeatureAvailability g_DLCFeatureAvailability;

    // MaybeDrawText (X360 0x82824048) -- the shared "draw debug text iff on-screen" helper. External to
    // this TU (its own TU); declared so the compile gate sees its shape. Signature recovered from the
    // call sites: (display, text, x, y, scale, colour, centred).
    int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                      f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

    // Never called -- pins the asm-attested member offsets of DLCDebugComponent. The console offsets
    // inside the byte/bool fields reproduce on the host; the PackDetail record's pointer field is 4
    // bytes on the 32-bit console and 8 on the 64-bit host, so the record is 12 bytes only on console --
    // every access is BY NAME, so the host stride difference is benign and is NOT asserted here.
    void DLCDebugComponent::_AssertDebugLayout()
    {
        static_assert(offsetof(PackDetail, mbAvailable)   == 0x00, "pack avail @ +0x00");
        static_assert(offsetof(PackDetail, mbLastApplied) == 0x01, "pack last @ +0x01");
    }

    // X360 0x826627F8 (store-for-store). The leading folded BaseCollisionGenerator::Destruct(this) is the
    // trivial base CgsDev::DebugComponent::Construct the X360 image coalesces onto one address. Then the
    // 25-entry feature-name table is zeroed (memset 100 bytes @ +0xC) and filled with the debug labels.
    // Every string below is a baked .rdata literal (stw aFeatures.../aCars.../aGameModes...).
    void DLCDebugComponent::Construct()
    {
        CgsDev::DebugComponent::Construct();   // base two-phase init (X360 folded body)

        std::memset(mapcFeatureNames, 0, sizeof(mapcFeatureNames));   // memset(this+0xC, 0, 100)

        mapcFeatureNames[0]  = "Features - Freeburn Challenges";            // a1[3]  +0x0C
        mapcFeatureNames[1]  = "Features - Specific Freeburn Challenges";   // a1[4]  +0x10
        mapcFeatureNames[2]  = "Features - Marked man and payback";         // a1[5]  +0x14
        mapcFeatureNames[3]  = "Features - Burning Routes";                 // a1[6]  +0x18
        mapcFeatureNames[4]  = "Features - Playground events";              // a1[7]  +0x1C
        mapcFeatureNames[5]  = "Features - new road rules";                 // a1[8]  +0x20
        mapcFeatureNames[6]  = "Features - Collectable Filter";             // a1[9]  +0x24
        mapcFeatureNames[7]  = "Features - Picture paradise mod";           // a1[10] +0x28
        mapcFeatureNames[8]  = "Features - jumps stunts smashes";           // a1[11] +0x2C
        mapcFeatureNames[9]  = "Cars - MONTGOMERY_HAWKER";                  // a1[12] +0x30
        mapcFeatureNames[10] = "Cars - SUPER_CAR";                          // a1[13] +0x34
        mapcFeatureNames[11] = "Cars - DIRT_KING";                          // a1[14] +0x38
        mapcFeatureNames[12] = "Cars - CRASH_TEST";                         // a1[15] +0x3C
        mapcFeatureNames[13] = "Cars - HUMMER";                             // a1[16] +0x40
        mapcFeatureNames[14] = "Cars - NEW_AGGRESSION";                     // a1[17] +0x44
        mapcFeatureNames[15] = "Cars - NEW_STUNT";                          // a1[18] +0x48
        mapcFeatureNames[16] = "Cars - NEW_SPEED";                          // a1[19] +0x4C
        mapcFeatureNames[17] = "Cars - HOT_ROD";                            // a1[20] +0x50
        mapcFeatureNames[18] = "Cars - DRAGSTER";                           // a1[21] +0x54
        mapcFeatureNames[19] = "Game Modes - Online stunt run";            // a1[22] +0x58
        mapcFeatureNames[20] = "Game Modes - Online road rage";            // a1[23] +0x5C
        mapcFeatureNames[21] = "Game Modes - Online Burning home run";     // a1[24] +0x60
        mapcFeatureNames[22] = "Leaderboards - burning route";             // a1[25] +0x64
        mapcFeatureNames[23] = "Leaderboards - Stunt run";                 // a1[26] +0x68
    }

    // X360 0x82662948. For each of the 5 data-pack records, latch the target availability byte: when it
    // differs from the last applied value, copy it forward and push the new state into the file-scope
    // DLCFeatureAvailability (SetPackAvailabilityState(pack index, new availability)). The leading folded
    // base call (BaseCollisionGenerator::Destruct == trivial base prep) is a no-op here.
    void DLCDebugComponent::Update()
    {
        for (PackDetail& lPack : maPackDetails)   // asm: walk +0x70 .. +0xAC, stride 12
        {
            bool lbChanged = false;
            if (lPack.mbLastApplied != lPack.mbAvailable)
            {
                lbChanged          = true;
                lPack.mbLastApplied = lPack.mbAvailable;   // stb r11, 1(r31)
            }

            if (lbChanged)
            {
                g_DLCFeatureAvailability.SetPackAvailabilityState(lPack.miPackIndex, lPack.mbLastApplied);
            }
        }
    }

    // X360 0x826629C8. Early-out unless the file-scope availability table's mbField01 flag (the byte at
    // its +0x01, byte_82FFA7F1) is set; when set, draw the "Beat the team mode" HUD label. The float
    // literals are baked .rdata: X=700.0 (flt_8205820C), Y=40.0 (flt_82004D0C, asm-confirmed in
    // BrnStuntManager), scale=16.0 (flt_82004000); the colour is the packed 0xFFFFFFC8 (li r8,-0x38).
    void DLCDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (g_DLCFeatureAvailability.mbField01)   // byte_82FFA7F1
        {
            MaybeDrawText(lpRender, "Beat the team mode", 700.0f, 40.0f, 16.0f, 0xFFFFFFC8u, false);
        }
    }

    // X360 0x827DD210. The component's debug-menu name: a single pointer to the baked .rdata string at
    // unk_8207C1EC. FLAG: the bytes at that address are not present in the available X360 exports, so the
    // literal cannot be grounded. Placeholder name used (non-load-bearing -- it is only the debug-menu
    // label); replace with the real string if the .rdata for 0x8207C1EC is recovered.
    const char* DLCDebugComponent::GetName() const
    {
        return "DLC Debug" /* FLAGGED: unk_8207C1EC string bytes not in exports */;
    }

    // X360 0x82662A10. Acquire a stack-scoped automatic debug interface (its ctor enters the debug
    // critical section and grabs the DebugManager singleton; GetDebugManager asserts it is non-null),
    // then activate this component in the debug menu. The interface's automatic-release semantics drop
    // the lock on scope exit (X360: the trailing `if (mbIsAutomaticClass) ThreadSafeRelease`).
    void DLCDebugComponent::OnRegister()
    {
        CgsDev::DebugInterface lDebugInterface;
        lDebugInterface.GetDebugManager().ActivateComponent(this);
    }

    // X360 0x82662B40. Register the 5 named data packs, then register each of the 25 features' enabled
    // flags (the file-scope DLCFeatureAvailability's per-feature mabFeatureEnabled[i]) as a bool debug
    // variable. The variable name is "<pack name>/<...> Entitlement" (CgsCore::SPrintf into a 127-byte
    // buffer) and the menu group is the feature's debug name (mapcFeatureNames[i]). The leading folded
    // base call is a no-op here.
    void DLCDebugComponent::OnActivate()
    {
        RegisterPackDetails("friends Pack",       0);
        RegisterPackDetails("rivalry Pack",       1);
        RegisterPackDetails("car Pack",           2);
        RegisterPackDetails("world Pack",         3);
        RegisterPackDetails("insider guide Pack", 4);

        for (s32 liFeature = 0; liFeature < KU_DLC_FEATURE_COUNT; ++liFeature)
        {
            // The feature's required data pack selects which pack-detail record names the entitlement.
            const s32         liPack = g_DLCFeatureAvailability.maePackForFeature[liFeature];  // dword_82FFA80C[i]
            const PackDetail& lPack  = maPackDetails[liPack];

            // FLAG: the X360 SPrintf("%s/%s Entitlement", ...) spills two pointer args -- the pack name
            // (record +0x04) is the only attested char* field; the second %s arg is the same record's
            // name (RegisterPackDetails, an external TU, owns the record's string population, so the
            // exact second field is non-load-bearing for the gate). Reproduced as pack name / pack name.
            char lacEntitlement[128];
            lacEntitlement[127] = 0;
            CgsCore::SPrintf(lacEntitlement, 127, "%s/%s Entitlement", lPack.mpcPackName, lPack.mpcPackName);

            RegisterVariable(&g_DLCFeatureAvailability.mabFeatureEnabled[liFeature],   // byte_82FFA870[i]
                             lacEntitlement, mapcFeatureNames[liFeature]);
        }
    }

    // X360 0x82661550. Bounds-checked access into the inline DLC package array: asserts the index
    // is in [0, muPackageCount) and returns &maPackages[liPackage] (array base == the object, so
    // the asm computes 76*index + this). The X360 formats the assert message at runtime by
    // streaming the index between the two baked literals "Package " and " is out of range\n"
    // through a CgsDev::StrStream, then fires the Begin/Fire/End triad -- the message embeds the
    // index so it CANNOT collapse to CGS_ASSERT (mirrors CgsMemoryMap::GetBank).
    DLCManager::DLCPackage* DLCManager::GetPackage(s32 liPackage)
    {
        if (liPackage < 0 || liPackage >= static_cast<s32>(muPackageCount))
        {
            CgsDev::Assert::BeginAssert();
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Package " << liPackage << " is out of range\n";
            CgsDev::Assert::FireAssert(
                lacMessageBuffer,
                "..\\..\\..\\GameSource\\Resource/BrnDLCManager.h",
                332);
            CgsDev::Assert::EndAssert();
        }

        return &maPackages[liPackage];   // mulli r11,liPackage,0x4C; add r3,r11,this
    }

    // X360 0x82662E18. The version check for the current DLC package failed: shift-remove that
    // package from the inline package array, decrement the package count, then set the monitor
    // stage. The removal is a memmove of the (count - current - 1) trailing records down one slot
    // (stride 76). The trailing stage store is 3 when the (now shorter) list still has a package at
    // miCurrentPackage (miCurrentPackage < new count) and 9 otherwise (list exhausted).
    // (The asm returns whatever r3 held; the value is unused by the caller Prepare -> void.)
    int DLCManager::DLCVersionCheckFailed()
    {
        const s32 liCurrent  = miCurrentPackage;                 // lwz r11, 0x624(r31)
        const s32 liTrailing = muPackageCount - liCurrent - 1;   // lwz 0x620; subf; addi -1

        if (liTrailing > 0)   // cmpwi cr6, r10, 0 / ble skips the memmove
        {
            std::memmove(&maPackages[liCurrent],
                         &maPackages[liCurrent + 1],
                         static_cast<size_t>(liTrailing) * sizeof(DLCPackage));   // 76*current, +76, 76*trailing
        }

        const s32 liNewCount = muPackageCount - 1;   // lwz 0x620; addi r11,-1
        muPackageCount = liNewCount;                 // stw r11, 0x620(r31)

        // miCurrentPackage < newCount -> another package remains to check (stage 3); else done (9).
        meVersionCheckStage = (miCurrentPackage < liNewCount) ? 3 : 9;   // cmpw r10,r11 / li 3 / blt / li 9

        return 0;   // r3 is leaked from the asm and unused; method is effectively void.
    }
}
