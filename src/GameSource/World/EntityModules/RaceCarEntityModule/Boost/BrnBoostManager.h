#pragma once

// ============================================================================
// BrnWorld::BoostManager -- the per-race-car boost state manager.
//   GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Three functions are ledgered:
//   Prepare                @ 0x822B8D08
//   SetBoostEarningEnabled @ 0x822A33B0
//   SetWrecking            @ 0x822B8E40
//
// KEYSTONE (layout + vtable-ordinal dispatch). No DWARF/leak home exists for
// BoostManager. The X360 bodies access the manager and its owning race-car entity
// purely through fixed byte offsets and VIRTUAL-TABLE ORDINALS:
//   * the manager stores its flags at +0x00 (boost-earning enabled) and dispatches
//     work to the owning entity held at +0x450;
//   * Prepare additionally constructs sub-managers at +0x10 / +0x160 / +0x2A0,
//     writes float/int state at +0x454/+0x458/+0x45C, sets a state byte at +0x04,
//     and emits two CgsDev debug-log lines;
//   * SetBoostEarningEnabled calls the entity's virtual slot +0xA8;
//   * SetWrecking reads/writes a wrecking flag at entity+0xBD and calls the entity's
//     virtual slot +0x20.
//
// Those vtable ordinals (+0x20, +0xA8) map to specific RaceCarEntityModule virtual
// methods whose SOURCE NAMES are not recoverable from these call sites, and the
// manager's preceding ~0x450 bytes of sub-manager state are likewise unattested.
// Reconstructing the dispatches would require fabricating either raw-offset casts
// (forbidden) or invented method names; per the project rules the bodies are left as
// honest, documented stubs that DO reproduce the recoverable observable stores (the
// boost-earning flag) without inventing the entity notifications. FLAG: this whole TU
// is a layout/vtable keystone -- fold it into the real RaceCarEntityModule boost home
// when that layout and the entity vtable map are recovered.
// ============================================================================

#include "types.hpp"

namespace BrnWorld
{

// Owning race-car entity, forward-declared. Its vtable layout (slots +0x20, +0xA8)
// and its wrecking flag (entity+0xBD) are not modelled by name here (see FLAG).
class RaceCarEntityModule;

// ⭐ NAMED 2026-08-11 (player-input wave). The "owning entity held at +0x450" the banner above
// describes IS a BrnWorld::BoostStrategy -- BrnBoostStrategy.h's own vtable table already records
// the identification from the other end ("slot 42 (+0xA8) SetBoostEarningEnabled <-
// BoostManager::SetBoostEarningEnabled @0x822A33B0", "slot 6 (+0x18) OnNearMiss <-
// BoostManager::OnNearMiss site", "slot 8 (+0x20) OnWrecked <- BoostManager::SetWrecking"), i.e.
// all three of this manager's vtable ordinals are BoostStrategy slots. Pointer-only use here, so
// a forward declaration is the documented exception; consumers include BrnBoostStrategy.h.
class BoostStrategy;

class BoostManager
{
public:
    // SetBoostEarningEnabled @ 0x822A33B0 -- notify the owning entity (virtual slot
    // +0xA8) then store the enabled flag. Only the flag store is reconstructed; the
    // entity notification is a documented keystone stub (see .cpp).
    void SetBoostEarningEnabled(bool lbEnabled);

    // SetWrecking @ 0x822B8E40 -- when turning wrecking on for an entity that is not
    // already wrecking, call the entity's wreck handler (virtual slot +0x20); always
    // store the wrecking flag into the entity. Keystone (see .cpp): the flag lives in
    // the entity at +0xBD and the handler is a vtable ordinal, neither modelled here.
    void SetWrecking(bool lbWrecking, s32 liParam);

    // Prepare @ 0x822B8D08 -- (re)initialise the boost manager and its sub-managers
    // for a fresh entity. Keystone (see .cpp): constructs sub-managers at unattested
    // offsets and dispatches through entity vtable ordinals.
    bool Prepare();

    // OnNearMiss -- notify the owning boost strategy of a near miss. The X360 inlines this
    // at NearMissManager::NearMissEvent into a dispatch through the pointer at +0x450, i.e.
    // `mpBoostStrategy->` vtable slot +0x18 == BoostStrategy::OnNearMiss (BrnBoostStrategy.h's
    // own slot table records the identification from the other end). ⚠ CORRECTED 2026-08-11:
    // this comment used to call +0x450 "the owning race-car entity" and slot +0x18 an entity
    // vtable slot. It is not an entity pointer -- three attested call sites pin +0x450 as
    // `BoostStrategy* mpBoostStrategy` (see GetBoostStrategy below), and all three of this
    // manager's vtable ordinals (+0x18, +0x20, +0xA8) are BoostStrategy slots.
    // Its source name is recovered from the Feb-2007 partial (BrnNearMissManager.cpp).
    // Declared here additively; its body is part of the deferred BoostManager keystone and
    // is not modelled in this build.
    void OnNearMiss();

    // ⭐ ADDED 2026-08-11 (player-input wave). RaceCarEntityModule::ProcessPlayerVehicleInput
    // @0x822FFE30 reads the boost button for the player's driver record as
    //     lwzx r3, r28, 0x17CE0 ; lwz r11, 0(r3) ; lwz r11, 0x4C(r11) ; mtctr ; bctrl
    // and 0x17CE0 (97504) == mBoostManager (module+96400, the receiver
    // `BoostManager::SetBoostEarningEnabled` is called with a few lines later) + 0x450, i.e. it
    // is exactly this pointer. Vtable slot 0x4C/4 == 19 == BoostStrategy::IsBoosting() const
    // (BrnBoostStrategy.h, @0x822A5E58). UpdateOutputBoostInfo and HandlePrepareForModeAction
    // reach the same pointer the same way (`BoostStrategy::OnModeStart(*(a1 + 97504), ...)`),
    // which is the independent corroboration that +0x450 holds a BoostStrategy*.
    //
    // ⚠️ NO WRITER ON THIS BUILD: BoostManager::Prepare is the console's only site that seeds it
    // and it is a documented keystone stub, so this stays NULL. Consumers must null-check and
    // say so; DELETE-WHEN Prepare lands.
    BoostStrategy*       GetBoostStrategy()       { return mpBoostStrategy; }
    const BoostStrategy* GetBoostStrategy() const { return mpBoostStrategy; }

private:
    // +0x00: boost-earning-enabled flag (SetBoostEarningEnabled stores here). Modelled
    // BY NAME; the remaining ~0x450 bytes of manager/sub-manager state up to the entity
    // pointer are a DEFERRED keystone and are NOT modelled.
    bool mbBoostEarningEnabled;

    // +0x450 (console): the owning boost strategy this manager dispatches every vtable ordinal
    // through. Named this wave (see GetBoostStrategy above). The ~0x450 bytes of sub-manager
    // state in front of it are still the deferred keystone, so the x64 offset is NOT the console
    // offset -- parity here is by name, as everywhere else in this tree.
    BoostStrategy* mpBoostStrategy = nullptr;
};

} // namespace BrnWorld
