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

    // OnNearMiss -- notify the owning race-car entity of a near miss. The X360 inlines this
    // at NearMissManager::NearMissEvent into a dispatch through the owning entity held at
    // +0x450 (entity vtable slot +0x18); its source name is recovered from the Feb-2007
    // partial (BrnNearMissManager.cpp). Declared here additively; its body is part of the
    // deferred BoostManager entity-vtable keystone and is not modelled in this build.
    void OnNearMiss();

private:
    // +0x00: boost-earning-enabled flag (SetBoostEarningEnabled stores here). Modelled
    // BY NAME; the remaining ~0x450 bytes of manager/sub-manager state up to the entity
    // pointer are a DEFERRED keystone and are NOT modelled.
    bool mbBoostEarningEnabled;
};

} // namespace BrnWorld
