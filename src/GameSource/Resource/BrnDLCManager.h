#ifndef GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
#define GAMESOURCE_RESOURCE_BRNDLCMANAGER_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (base of DLCDebugComponent)

namespace CgsDev { struct Debug2DImmediateRender; }

// ============================================================================
// GameSource/Resource/BrnDLCManager.h
//
// MINIMAL SLICE -- models only the downloadable-content availability flags that
// the boot-legal flow touches. The full DLC manager (BrnResource::DLCManager and
// its package list, the DLC debug component, and the feature-availability table)
// lands when those TUs are reconstructed; GROW this header then, do NOT fork the
// DLC types elsewhere.
//
// Shape recovered from the X360 asm of DLCBeatTheTeamGame::SetEnabledState
// (@0x82472CA8) and its baked assert string at BrnDLCManager.h:405
// ("!(mbIsEnabled && !mbIsAvailable)"): each downloadable game is a two-flag
// record -- availability (whether the content is installed/owned) and enabled
// (whether the gameplay path that uses it is switched on). The invariant the
// assert enforces is that a game cannot be enabled while it is unavailable.
// ============================================================================

namespace BrnResource
{

// ---------------------------------------------------------------------------
// Downloadable-content data packs.
//
// GetPackMask / SetPackAvailabilityState bound-check the pack index against
// these two limits (X360 asm: cmpwi lPack,0 / cmpwi lPack,5 with the baked
// assert strings "lPack >= E_DLC_DATA_PACK_START" and
// "lPack < E_DLC_DATA_PACK_COUNT"). START is the inclusive lower bound (0) and
// COUNT is the exclusive upper bound (5).
// ---------------------------------------------------------------------------
enum E_DLC_DATA_PACK
{
    E_DLC_DATA_PACK_START = 0,
    E_DLC_DATA_PACK_COUNT = 5
};

// Number of feature slots in the availability table. The X360 Construct
// (@0x82662C48) memsets a 100-byte (25-dword) block at +0x1C and writes a
// parallel 25-byte bool block at +0x80; both run 0..24.
enum { KU_DLC_FEATURE_COUNT = 25 };

// ---------------------------------------------------------------------------
// DLCFeatureAvailability
//
// Availability table for downloadable content. Holds, per data pack, the bit
// the pack contributes to the running availability mask, and, per feature, the
// pack it requires plus a default-enabled flag.
//
// Layout recovered store-for-store from BrnResource::DLCFeatureAvailability::
// Construct (@0x82662C48), GetPackMask (@0x82661628) and
// SetPackAvailabilityState (@0x826616A0). Total size 0x99 (153) bytes.
//   +0x00 mbConstructed      byte, set 1 by Construct
//   +0x01 mbField01          byte, set 0 by Construct
//   +0x04 muAvailabilityMask dword, the running mask SetPackAvailabilityState mutates
//   +0x08 mauPackMask[5]     dword per pack {1,2,4,8,16}; GetPackMask returns mauPackMask[lPack]
//   +0x1C maePackForFeature[25] dword per feature (which pack it requires)
//   +0x80 mabFeatureEnabled[25] byte per feature (default-enabled flag)
// ---------------------------------------------------------------------------
class DLCDebugComponent;   // same-TU debug component reaches the file-scope table directly

class DLCFeatureAvailability
{
    // The DLC debug component lives in the same TU as the file-scope availability table and, in the
    // X360 image, reads/writes its members straight off the static's address (byte_82FFA7F0 etc.).
    friend class DLCDebugComponent;

public:
    // X360 0x82662C48. Zeroes the mask + table, then writes the default per-pack
    // masks, per-feature pack indices and per-feature enabled flags. (The asm's
    // int return is just memset's r3 leaking through Hex-Rays; the method is void.)
    void Construct();

    // X360 0x82661628. Returns the bit mask the given data pack contributes to the
    // availability mask. Asserts lPack is in [E_DLC_DATA_PACK_START, E_DLC_DATA_PACK_COUNT).
    u32 GetPackMask(s32 liPack) const;

    // X360 0x826616A0. Sets or clears the given pack's bit in the availability mask.
    // When lbAvailable is true the pack's bit is OR'd in; otherwise the mask is AND'd
    // with the pack's bit cleared. Asserts the same pack-index bounds.
    void SetPackAvailabilityState(s32 liPack, bool lbAvailable);

private:
    // Never called; defined in BrnDLCManager.cpp to pin member offsets via offsetof.
    static void _AssertLayout();

    bool mbConstructed;                          // +0x00
    bool mbField01;                              // +0x01
    u8   mPad02[2];                              // +0x02 (alignment to the +0x04 dword)
    u32  muAvailabilityMask;                     // +0x04
    u32  mauPackMask[E_DLC_DATA_PACK_COUNT];     // +0x08 .. +0x18
    s32  maePackForFeature[KU_DLC_FEATURE_COUNT];// +0x1C .. +0x7C
    bool mabFeatureEnabled[KU_DLC_FEATURE_COUNT];// +0x80 .. +0x98
};

// A single downloadable "Beat The Team" online game's availability/enabled state.
// Layout recovered from SetEnabledState: mbIsAvailable @ +0x00, mbIsEnabled @ +0x01.
class DLCBeatTheTeamGame
{
public:
    bool IsAvailable() const { return mbIsAvailable; }
    bool IsEnabled() const   { return mbIsEnabled; }

    // X360 0x82472CA8. Stores the enabled flag, then asserts that the content is not
    // enabled while it is unavailable (BrnDLCManager.h:405). The assert is non-fatal
    // (the binary stores the flag and returns regardless).
    void SetEnabledState(bool lbEnabled);

private:
    bool mbIsAvailable; // +0x00
    bool mbIsEnabled;   // +0x01
};

// ---------------------------------------------------------------------------
// DLCDebugComponent
//
// The in-game debug menu + HUD overlay for the downloadable-content system. It
// derives from the real CgsDev::DebugComponent and, through the debug menu, lets
// you toggle the per-feature "entitlement" flags of the file-scope
// DLCFeatureAvailability instance and watch the per-pack availability records.
//
// SOURCE-OF-TRUTH: bodies reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
//   Construct   @ 0x826627F8  (caller BrnResource::GameDataModule::Construct)
//   Update      @ 0x82662948
//   RenderHUD   @ 0x826629C8
//   OnRegister  @ 0x82662A10
//   OnActivate  @ 0x82662B40
//   GetName     @ 0x827DD210
// No DecFIGS DWARF for this class; layout is asm-attested (offsets noted below).
//
// LAYOUT (members BY NAME + SEQUENCE; console 32-bit `this` offsets noted for
// provenance only -- every access is by member name). The base CgsDev::
// DebugComponent occupies console +0x00..+0x0B (vtable + mbActive + list-next), so
// the first member below sits at console +0x0C.
//   +0x0C mapcFeatureNames[25]  const char* per feature (Construct stores 25 string
//                               literals at a1[3]..a1[27]; memset 100 bytes @+0xC).
//   +0x70 maPackDetails[5]      one 12-byte record per data pack (Update walks the
//                               5 records from +0x70 to +0xAC, stride 12).
// Total console size 0xAC (172) bytes.
// ---------------------------------------------------------------------------
class DLCDebugComponent : public CgsDev::DebugComponent
{
public:
    DLCDebugComponent() {}

    // X360 0x826627F8. Two-phase construct: the folded base CgsDev::DebugComponent::
    // Construct (the X360 image coalesces the trivial base body onto the
    // BaseCollisionGenerator::Destruct address), zero the 25-entry feature-name table,
    // then store the 25 debug feature-name string literals.
    void Construct();

    // X360 0x82662948. Per-frame: for each of the 5 pack-detail records, if the target
    // availability byte changed since last frame, push the new state into the file-scope
    // DLCFeatureAvailability via SetPackAvailabilityState.
    virtual void Update();

    // X360 0x826629C8. Draw the "Beat the team mode" HUD label iff the file-scope
    // DLCFeatureAvailability's mbField01 flag is set.
    virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender);

protected:
    // X360 0x827DD210. The component's debug-menu name.
    virtual const char* GetName() const;

    // X360 0x82662A10. Acquire a (stack-scoped) debug interface, assert the manager exists,
    // and activate this component in the debug menu.
    virtual void OnRegister();

    // X360 0x82662B40. Register the 5 data packs (RegisterPackDetails) then, for each of the
    // 25 features, register the file-scope availability table's per-feature enabled flag as a
    // bool debug variable grouped under the feature name.
    virtual void OnActivate();

private:
    // X360 0x82662B40 helper (own TU -- declaration only here). Populate one pack-detail
    // record (name + index) and seed its availability state. The X360 keeps the body outside
    // this TU's function set.
    void RegisterPackDetails(const char* lpcPackName, s32 liPackIndex);

    // One downloadable data pack's debug record. 12 bytes (Update stride). Fields named from
    // the asm: Update reads +0/+1 as the target/last availability bytes and +8 as the pack
    // index; OnActivate reads +4 as a char* (the pack name) for the entitlement label.
    struct PackDetail
    {
        bool        mbAvailable;     // console +0x00  target availability (RegisterPackDetails / menu)
        bool        mbLastApplied;   // console +0x01  last value pushed into DLCFeatureAvailability
        u8          mPad02[2];       // console +0x02  alignment to the +0x04 pointer
        const char* mpcPackName;     // console +0x04  pack display name (set by RegisterPackDetails)
        s32         miPackIndex;     // console +0x08  data-pack index (0..4)
        // (console record = 12 bytes; the pointer widens to 8 on the 64-bit host -> access BY NAME only)
    };

    // Never called; pins the asm-attested member offsets via offsetof.
    static void _AssertDebugLayout();

    const char* mapcFeatureNames[KU_DLC_FEATURE_COUNT];  // +0x0C .. +0x6F (25 ptrs, 100 bytes)
    PackDetail  maPackDetails[E_DLC_DATA_PACK_COUNT];     // +0x70 .. +0xAB (5 records, 60 bytes)
};

// ============================================================================
// BrnResource::DLCManager -- the runtime DLC package manager. Reconstructed from
// BURNOUT_X360_ARTIST.XEX. Holds an inline array of DLC package records + a small
// version-check/monitor state machine. This wave bodies GetPackage @0x82661550 and
// DLCVersionCheckFailed @0x82662E18; the sibling lifecycle methods are declared for
// shape (bodies land with their own TUs).
//
// Capacity is an inference from floor(0x500 / 0x4C); the asm never attests the array bound.
enum { KU_DLC_PACKAGE_CAPACITY = 16 };

class DLCManager
{
public:
    // One 76-byte (0x4C) DLC package record. Field layout is NOT attested by this batch
    // (GetPackage only returns a pointer into the array, DLCVersionCheckFailed only shifts whole
    // records), so the record is an opaque correctly-sized byte span (stride from mulli ...,0x4C).
    struct DLCPackage
    {
        u8 mOpaque[76];   // X360-attested stride (0x4C)
    };

    // Sibling DLCManager methods (declared for shape; bodies land with their own TUs).
    void Construct();                                    // DWARF :62
    bool Prepare(class AllocatorList* lpAllocatorList);  // DWARF :66 -- calls DLCVersionCheckFailed
    bool Release();                                      // DWARF :70
    void Destruct();                                     // DWARF :74
    void StartDLCMonitor();                              // DWARF :77
    s32  GetDLCVersion() const;                          // DWARF :80

    // X360 0x82661550. Bounds-checked access into the inline package array; asserts
    // 0 <= liPackage < muPackageCount (runtime-streamed "Package <i> is out of range\n" message)
    // and returns &maPackages[liPackage].
    DLCPackage* GetPackage(s32 liPackage);

    // X360 0x82662E18. The current package's version check failed: shift-remove it from the array,
    // decrement muPackageCount, and advance meVersionCheckStage (3 = more to check, 9 = done).
    // (Returns int only because the asm leaks r3; the value is unused -> effectively void.)
    int DLCVersionCheckFailed();

private:
    DLCPackage  maPackages[KU_DLC_PACKAGE_CAPACITY];  // +0x000  inline package array (base == this); capacity inferred
    u8          mPad04C0[0x500 - KU_DLC_PACKAGE_CAPACITY * 76];  // +0x4C0  gap up to the stage dword (0x40 bytes)
    s32         meVersionCheckStage;                  // +0x500
    u8          mPad0504[0x620 - 0x504];              // +0x504  unmodelled fields up to the count dword
    s32         muPackageCount;                        // +0x620
    s32         miCurrentPackage;                      // +0x624
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
