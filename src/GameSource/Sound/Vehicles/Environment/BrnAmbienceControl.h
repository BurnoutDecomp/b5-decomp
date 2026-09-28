#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"             // CgsSound::Utils::DataPoint
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceControl
//   Originally declared beside AmbienceEffect (BrnAmbienceEffect.h); kept in its
//   own header here.
//
// The per-car environmental ambience CONTROL. Every few seconds it picks the ambience
// region under the listener: a tunnel, heavy or light traffic when one applies, else the
// district byte of the streamed 2D ambience map ("Ambiences" in
// sound\regions\ambiences.dat). AmbienceEffect streams the bed for the chosen region.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct EnclosureControl;

struct AmbienceControl : public BrnSound::Logic::BrnEffectControl
{
    // Region value meaning "no ambience region yet".
    static constexpr u8 KU8_INVALID_REGION = 255;

    // The console factory writes only mRegion (zero); the handle, the enclosure link and
    // the timer are cleared here for host determinism and set properly by Attach.
    AmbienceControl()
        : mMap2d()
        , mRegion(0)
        , mpEnclosureControl(nullptr)
        , mfTimeSinceUpdate(0.0f)
    {
        mMap2dResource.Clear();
    }
    virtual ~AmbienceControl();

    // RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );

    s32 GetController(s32 aiIndex) override;
    void AttachController(CgsSound::Logic::EffectBase* apController) override;
    void UpdateParams(f32 afTimeStep) override;
    bool Attach() override;
    void SetupLoadData() override;

    const CgsSound::Utils::DataPoint<u8>& GetRegion() const { return mRegion; }

private:
    bool SelectSpecialAmbience(u8& aru8Ambience) const;

public:
    // Members in declaration order.
    CgsWorld::WorldMap2D           mMap2d;
    CgsResource::ResourceHandle    mMap2dResource;
    CgsSound::Utils::DataPoint<u8> mRegion;
    EnclosureControl*              mpEnclosureControl;
    f32                            mfTimeSinceUpdate;
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H
