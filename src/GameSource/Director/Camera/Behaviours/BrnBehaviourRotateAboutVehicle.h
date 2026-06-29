#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H

#include "types.hpp"

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h
//
// BrnDirector::Camera::BehaviourRotateAboutVehicle -- the "rotate about vehicle" camera
// behaviour: it sweeps the camera in an arc around the tracked car (used for orbit/reveal
// shots). HOME for the one BehaviourRotateAboutVehicle class slice this TU bodies: a member
// accessor that returns the address of an embedded sub-object at +0x50. The full behaviour
// (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with their
// own TUs; this header models only the slice this accessor needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is the accessor @0x821FB410:
//     addi r3, r3, 0x50 ; blr
// i.e. it returns `this + 0x50` -- the address of a member sub-object embedded at +0x50. The
// shipped symbol carries no method name and the function has no recorded xrefs, so the member's
// precise type/role is unrecoverable; it is modelled here as a named embedded member at the
// asm-attested offset and returned BY NAME via its address.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

struct Camera;   // the per-frame camera the behaviour produces (full type: Camera.h)

class BehaviourRotateAboutVehicle
{
public:

    // FLAG: the member sub-object returned by the accessor. The accessor @0x821FB410 is just
    //   `return this + 0x50`, returning the address of the member embedded at +0x50. The
    //   shipped binary records no name for the accessor and no callers, so the member's type
    //   is unrecoverable; it is modelled as a named, opaque-bytes sub-object placed at the
    //   asm-attested offset (+0x50 is authoritative; the internal layout is intentionally not
    //   modelled, only its address is taken). Replace with the real member type when the full
    //   behaviour TU lands.
    struct EmbeddedSubObject
    {
        u8 maOpaque[1];   // opaque head; only the sub-object's address is used
    };

    // Accessor returning the address of the +0x50 member sub-object. @0x821FB410
    //   (addi r3, r3, 0x50 ; blr  ->  return &maEmbeddedSubObject).
    EmbeddedSubObject* GetEmbeddedSubObject() { return &maEmbeddedSubObject; }

    // Configure the behaviour from a named-parameter block. X360-attested @0x821F55B8 (the
    // online car-select arbitrator state allocates this behaviour and immediately hands it the
    // "look around car" parameter block from the NamedParameters bank). The behaviour stores /
    // applies the block; modelled here as the named setter taking the parameter block as an
    // opaque pointer (its interior layout is owned by the parameter-bank TU). DECLARATION-ONLY:
    // the body lands with this behaviour's own TU; the per-TU cl /c gate does not link.
    void SetParameters(const void* lpParameters);

    // The camera this behaviour produced this frame. The arbitrator states copy it into their
    // own mCamera while this behaviour is driving them (the X360 reaches it via the manager pool
    // slot the BehaviourHandle resolves to -- sub_821FDF38 reads slot+0x10, the same role the
    // ICE-anim behaviour exposes as GetProducedCamera). DECLARATION-ONLY: the body (and the
    // produced-camera member it returns) land with this behaviour's own TU; modelled here BY
    // NAME so consumers never reach the camera by offset.
    const Camera& GetProducedCamera() const;

private:

    // The vtable/base head occupies +0x00; the addressed member sub-object sits at +0x50.
    // Reserved span places it exactly. The rest of the rig lands with the full behaviour TU.
    void*             mpVTable;                            // +0x00  behaviour vtable (opaque base head)
    u8                maReserved[0x50 - sizeof(void*)];    // +0x08(host) .. +0x4F (rig members not modelled here)
    EmbeddedSubObject maEmbeddedSubObject;                 // +0x50  the addressed member sub-object
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
