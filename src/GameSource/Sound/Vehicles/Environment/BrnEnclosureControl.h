#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H

#include "types.hpp"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"   // BrnTrigger::GenericRegion::Type, KI_FIRST_SOUND_ENCLOSURE

// =============================================================================
// BrnSound::Vehicles::Environment::EnclosureControl
//   GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: EnclosureControl : public
// BrnEffectObject. Maps a trigger region type to an enclosure-index and owns the
// enclosure (tunnel/underpass) reverb transition.
//
// It tracks, for the player car, which sound-enclosure trigger regions (tunnel,
// overpass, bridge, ...) are active at the car and just ahead of it. The reverb and
// ambience effects read the at-entity set.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines { struct PhysicsControl; }
namespace Environment
{

// Where a trigger query was made relative to the car.
extern bool KB_SHOW_STATIC_ENVIRONMENT; // ARTIST 82FFB8C8, shared with static passbys

enum eTriggerPosition
{
    E_TRIGGER_POSITION_AT_ENTITY       = 0,
    E_TRIGGER_POSITION_AHEAD_OF_ENTITY = 1,
    E_TRIGGER_POSITION_COUNT           = 2,
};

// The active sound-enclosure region set of one trigger query and its previous value.
// Bit i stands for region type KI_FIRST_SOUND_ENCLOSURE + i.
struct EntityTriggerInfo
{
    void Reset()
    {
        muActiveTriggers = 0;
        muPrevTriggers   = 0;
    }

    BrnTrigger::GenericRegion::Type GetChangeType() const; // ARTIST 82685D88

    bool HasChanged() const { return muActiveTriggers != muPrevTriggers; }

    bool IsTypeActive(BrnTrigger::GenericRegion::Type aeType) const
    {
        return (muActiveTriggers &
                (1u << (static_cast<s32>(aeType) - BrnTrigger::KI_FIRST_SOUND_ENCLOSURE))) != 0;
    }

    u32 muActiveTriggers;
    u32 muPrevTriggers;
};

struct EnclosureControl : public BrnSound::Logic::BrnEffectControl
{
    EnclosureControl()
        : mpPhysicsControl(nullptr)
        , mfTimeSinceTrigger(0.0f)
    {
        maTriggerInfo[E_TRIGGER_POSITION_AT_ENTITY].Reset();
        maTriggerInfo[E_TRIGGER_POSITION_AHEAD_OF_ENTITY].Reset();
    }
    virtual ~EnclosureControl();    // anchor for the vector deleting destructor @ 0x826B94A8

    // @ 0x82685FA0 -- map a region type (19..31) to an enclosure index (pure; `this` unused).
    int ConvertRegionTypeToIndex( int liRegionType ) const;
    // @ 0x826D0A30 -- allocate + construct factory. Returns the EffectObject* base view.
    static CgsSound::Logic::EffectControl* Create( bool lbFlavour );

    s32 GetController(s32 aiIndex) override;
    void AttachController(CgsSound::Logic::EffectBase* apController) override;
    bool Attach() override;
    void UpdateParams(f32 afTimeStep) override;
    void ProcessTriggerAction(const BrnGameState::GameStateModuleIO::SoundTriggerAction& arAction,
                              eTriggerPosition aePosition);
    void DrawDebug() const;

    const EntityTriggerInfo& GetTriggerInfo(eTriggerPosition aePosition) const
    {
        return maTriggerInfo[aePosition];
    }

    // Members in declaration order.
    EntityTriggerInfo                            maTriggerInfo[E_TRIGGER_POSITION_COUNT];
    BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl;
    f32                                          mfTimeSinceTrigger;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H
