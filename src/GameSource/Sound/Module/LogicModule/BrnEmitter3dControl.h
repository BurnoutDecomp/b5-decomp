#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_3D_CONTROL_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_3D_CONTROL_H

#include "GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.h"

namespace BrnSound
{
namespace Logic
{
namespace World
{

// State-7 specialization of the shared positional control.  DWARF attests no
// additional data members; the specialization supplies the registry identity.
class Emitter3dControl : public CgsSound::Logic::Cgs3dEffectControl
{
public:
    Emitter3dControl();
    virtual ~Emitter3dControl();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
        GetTypeInfo() const;
    virtual const char* GetTypeName() const;

    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>*
        GetStaticTypeInfo();
    static CgsSound::Logic::EffectControl* CreateObject(u32 au32Param);
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif
