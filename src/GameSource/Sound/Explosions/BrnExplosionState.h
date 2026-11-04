#ifndef BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H
#define BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// ARTIST 0x820B2B3C is the instance vtable. Its type info and factory use
// descriptor 0x82F2F9AC. The leaf inherits State teardown and parameter updates.

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

// BrnExplosionState.h:34 (DWARF).
struct ExplosionState : public BrnSound::Logic::BrnState
{
    ExplosionState() {}

    virtual ~ExplosionState();

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetTypeInfo() const;
    virtual const char*                                            GetTypeName() const;

    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* GetStaticTypeInfo();
    static CgsSound::Logic::State* CreateObject(u32 luFlavour);

    // @ 0x826891B8 — null-asserts the attachment pointer ("lpvAttachment"). Bodied
    // in this TU's .cpp. DWARF: virtual void Attach(void*) (BrnExplosionState.cpp:66).
    virtual void Attach(void* lpvAttachment);
};

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_STATE_H
