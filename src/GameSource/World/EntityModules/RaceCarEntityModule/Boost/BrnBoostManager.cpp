#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostManager.h"

// ============================================================================
// BrnWorld::BoostManager -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnBoostManager.h for the
// KEYSTONE rationale (unattested manager layout + RaceCarEntityModule vtable-ordinal
// dispatch). The bodies below reproduce the recoverable observable stores and leave
// the unrecoverable entity-vtable dispatches as honest, documented stubs.
//
// dep_flags: BoostManager's full layout and the RaceCarEntityModule virtual-method
// map (slots +0x20, +0xA8) are DEFERRED keystones -- not homed in this group.
// ============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// SetBoostEarningEnabled @ 0x822A33B0
//
//   entity = *(this + 0x450);
//   (*(*entity + 0xA8))(entity);   // notify the entity (vtable ordinal +0xA8)
//   *(bool*)this = enabled;        // store the flag at +0x00
//
// KEYSTONE: the entity pointer (+0x450) and the +0xA8 virtual slot are not modelled
// by name. The recoverable observable effect -- storing the enabled flag at +0x00 --
// is reproduced; the entity notification is left as a documented stub rather than a
// fabricated vtable call.
// ---------------------------------------------------------------------------
void BoostManager::SetBoostEarningEnabled(bool lbEnabled)
{
    // KEYSTONE STUB: notify the owning entity via virtual slot +0xA8 (deferred).
    mbBoostEarningEnabled = lbEnabled;   // stb r30, 0(this)
}

// ---------------------------------------------------------------------------
// SetWrecking @ 0x822B8E40
//
//   entity = *(this + 0x450);
//   if (wrecking && !entity[0xBD])
//       (*(*entity + 0x20))(entity, param);   // entity wreck handler (vtable +0x20)
//   entity[0xBD] = wrecking;                   // store wrecking flag in the entity
//
// KEYSTONE: the wrecking flag lives in the owning entity at +0xBD and the handler is
// an unnamed vtable ordinal (+0x20); neither the entity layout nor its vtable map is
// modelled here. There is NO recoverable store on the BoostManager itself (the writes
// target the entity), so the body is a documented stub.
// ---------------------------------------------------------------------------
void BoostManager::SetWrecking(bool lbWrecking, s32 liParam)
{
    // KEYSTONE STUB: guard on entity+0xBD, dispatch entity vtable slot +0x20, then
    // store the wrecking flag into the entity. Deferred (unattested entity layout).
    (void)lbWrecking;
    (void)liParam;
}

// ---------------------------------------------------------------------------
// Prepare @ 0x822B8D08
//
//   construct sub-manager @ +0x10  (virtual ctor dispatch)
//   construct sub-manager @ +0x160 (virtual ctor dispatch)
//   construct sub-manager @ +0x2A0 (virtual ctor dispatch)
//   *(float*)(this+0x45C) = 0.0; *(int*)(this+0x454)=0; *(int*)(this+0x458)=0;
//   if (CgsDev::Message::gxMessageFilterFlags & 1) <emit "leVersion: " + newline>
//   if (CgsDev::Message::gxMessageFilterFlags & 1) <emit "****...****\n">
//   *(this+0x450) = sub-manager@+0x2A0; *(int*)(this+4) = 5;
//   (sub-manager@+0x2A0 vtable[0])(...);
//   (*(this+0x450) vtable[0xA8])(*(this+0x450), *(byte*)this);
//   *(byte*)this = 0;
//   return 1;
//
// KEYSTONE: this body constructs three sub-managers at unattested offsets and
// dispatches through several vtable ordinals on both the sub-managers and the entity.
// None of those types/offsets are DWARF-attested, so a faithful reconstruction is not
// possible without fabricating layout. The body is a documented stub returning the
// X360 success value (1) so callers compile and link.
// ---------------------------------------------------------------------------
bool BoostManager::Prepare()
{
    // KEYSTONE STUB: sub-manager construction + entity/sub-manager vtable dispatch +
    // CgsDev debug logging not reconstructed (deferred; unattested layout).
    return true;   // X360 `li r3, 1`
}

} // namespace BrnWorld
