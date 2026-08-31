#ifndef BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_MANAGER_H
#define BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_MANAGER_H

#include "types.hpp"
#include "GameSource/Sound/Vehicles/BrnVehicleStateManager.h"
#include "SharedClasses/Sound/World/BrnSoundWorldScene.h"
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleStateManager
//   GameSource/Sound/Vehicles/BrnPlayerVehicleStateManager.{h,cpp}
//   (canonical home -- derived from the X360 mangled name
//    BrnSound::Vehicles::PlayerVehicleStateManager; sits beside the committed sibling
//    AIVehicleStateManager in GameSource/Sound/Vehicles/.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PlayerVehicleStateManager is the sound-logic state manager that owns the PLAYER
// car's engine + vehicle audio -- the deepest vehicle-audio cascade (the player
// engine voices, the per-component content banks, the world-scene-driven 3D voice
// placement). It is one of the 9 managers the SoundLogicModule factory
// CreateStateManagers (0x826AFEF8) creates via CreateStateMan.
//
// BASE CHAIN: PlayerVehicleStateManager : public BrnSound::Logic::BrnStateManager
//   (-> CgsSound::Logic::StateManager primary base + BrnSound::Logic::
//    IResourceRequester sub-object). Evidence: the ctor @ 0x82700BE8 installs a
//   primary vtable @ +0 (off_820B87A0) AND a secondary sub-object vtable @ +0x90
//   (*(result+144) = off_820B8798, after a transient off_820AB608) -- the
//   IResourceRequester sub-object vptr -- and the dtor @ 0x82700D30 tears down the
//   base CgsSound::Logic::StateManager::RegisteredContent ObjectPool at +0xC. Same
//   primary shape as the committed siblings AIVehicleStateManager / PassbyStateManager.
//
//   FLAG (THIRD sub-object vtable @ +0x98 NOT modelled): unlike the AIVehicle/Passby
//   template (which install only +0/+0x90), this ctor ALSO installs a third vtable
//   *(result+152) = off_820B1738 @ +0x98 -- i.e. PlayerVehicleStateManager carries an
//   ADDITIONAL secondary base sub-object beyond IResourceRequester (the same +0x98
//   slot the committed EmitterStateManager carries; off_820B1738 is the shared
//   sub-object vtable both install). That extra base is NOT identified or modelled in
//   this minimal shell (it pulls a domain interface this slice does not reconstruct);
//   the shell derives ONLY BrnStateManager. Consequence: this leaf is layout-
//   incomplete vs the X360 (the +0x98 base sub-object is absent). The conductor must
//   add the real second secondary base at integration if the per-frame path that uses
//   it is brought up; the boot path (Prepare) does not exercise it.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 object is 1152 bytes (0x480)
// behind 4-byte pointers/vptrs (CreateObject @ 0x827022D8 allocates 1152); on the
// 64-bit host the layout differs, so members are pinned BY NAME only and the 0x480
// size / absolute offsets are NOT static_asserted. The ctor builds SEVEN refcounted
// content sub-objects at +0x42C..+0x47C (each {&off_820B3250 vtable, 0, 0}) and seeds
// a tail of scalar fields; no recovered body in this slice names an individual data
// member by a recovered field name, so the player-vehicle voice/content state is
// deferred (see FLAG) and a single opaque pad honestly names it.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

class PlayerVehicleStateManager : public BrnSound::Vehicles::VehicleStateManager
{
public:
    // PlayerVehicleStateManager @ 0x82700BE8. Forwards to the BrnStateManager base
    // ctor and builds the seven content sub-objects.
    PlayerVehicleStateManager();

    // ~PlayerVehicleStateManager @ 0x82700D30 (the X360 `vector deleting destructor`).
    virtual ~PlayerVehicleStateManager();

    // ---- RTTI hooks (the per-class descriptor + factory). STATIC GetStaticTypeInfo
    // / CreateObject so &CreateObject is storable in ClassTypeInfo<StateManager>::
    // mpfnCreateObject (the X360 CreateObject @ 0x827022D8 never touches an instance
    // -- its int arg is the operator-new flavour selector, not `this`). ----
    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetTypeInfo() const;
    virtual const char* GetTypeName() const;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::StateManager>* GetStaticTypeInfo();
    static CgsSound::Logic::StateManager* CreateObject( u32 luType );                     // @ 0x827022D8

    // ---- boot + lifecycle virtuals ----
    virtual bool Prepare();                       // @ 0x826EF428
    virtual bool Release();                       // @ 0x826EFBB8
    virtual void UpdateParams(f32 af32DeltaTime); // @ 0x826EF7E8

    // ---- IResourceRequester overrides (pure in IResourceRequester; BrnStateManager
    // declares but does not body them, so the concrete leaf must override+body them). ----
    virtual void ResourcesAreReady();             // @ 0x826CA4E0

private:
    BrnSound::Logic::World::SoundWorldScene mSoundScene;
    CgsSound::Logic::Content mCsisDeformationInterface;
    CgsSound::Logic::Content mCsisBoostInterface;
    CgsSound::Logic::Content mCsisSkidInterface;
    CgsSound::Logic::Content mCsisInAirInteface;
    CgsSound::Logic::Content mCsisSurfaces;
    CgsSound::Logic::Content mCsisTurbo;
    CgsSound::Logic::Content mCsisGearWhine;
};

} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_PLAYER_VEHICLE_STATE_MANAGER_H
