#ifndef CGS_SOUND_LOGIC_CGSSTATEMANAGER_H
#define CGS_SOUND_LOGIC_CGSSTATEMANAGER_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/CgsMemBase.h"   // CgsSound::MemBase

// CgsSound::Logic::StateManager - the sound-logic state manager. This is a large
// polymorphic keystone class (~50 member functions across CgsStateManager.cpp);
// the ONLY function homed here is the leaf accessor IsStateAlias() at 0x82680D48,
// recovered from the X360 pseudocode/asm:
//
//   0x82680D48  lwz    r11, 0x14(r3)   ; r11 = this->meMapState
//   0x82680D4C  subf   r11, r4, r11    ; r11 = meMapState - liState
//   0x82680D50  cntlzw r11, r11        ; count-leading-zeros
//   0x82680D54  extrwi r3, r11, 1,26   ; r3 = (meMapState == liState) ? 1 : 0
//   0x82680D58  blr
//
// i.e. IsStateAlias(liState) returns (meMapState == liState). The Hex-Rays
// pseudocode renders `this` as a first int parameter `a1` and the real argument as
// `a2`; the DWARF declares the source-level signature `virtual bool
// IsStateAlias(int32_t)` (the comparand is the single int argument). The member
// read at +0x14 is meMapState (DWARF CgsStateManager.h:341): with MemBase's vptr at
// +0x00 and the four leading f32 fields at +0x04/+0x08/+0x0C/+0x10, meMapState
// lands at +0x14 in the X360 32-bit layout.
//
// FLAG (faithful, not byte-identical; partial keystone view): this header models
// ONLY the leading scalar members up to meMapState (all non-pointer 4-byte fields,
// so their relative order is portable) plus the IsStateAlias accessor, so the one
// owned function can be bodied with member-by-name access and no raw-offset cast.
// The remaining StateManager surface (state list, prepare/release state machine,
// content registry ObjectPool, the many virtuals) is NOT modelled here and is left
// to the full owning keystone TU. Absolute member offsets are deliberately NOT
// static_asserted: the host 64-bit vptr differs in width from the X360 32-bit
// vptr, so only the by-name semantic (meMapState == liState) is reproduced.
// FLAG (committed-home coexistence -- conductor please note): a sibling header
// CgsStateManagerRegisteredContent.h gives a separate partial view of this same
// enclosing class purely to host the nested RegisteredContent type, and the old
// CgsStateManager.cpp stub modelled IsStateAlias against an opaque mPad[20] +
// miAliasState. This header supersedes that stub with the DWARF-named meMapState
// member; the views never share a TU. When StateManager gets a full owning header,
// fold this accessor, the nested RegisteredContent, and the rest of the class
// together.
namespace CgsSound
{
namespace Logic
{

// CgsStateManager.h:313 (DWARF). Per-class RTTI descriptor for the StateManager
// family, templated on the leaf class. Same shape as the EffectBase/State-family
// descriptor; declared here too because CgsStateManager.h is never co-included with
// the other RTTI homes in a single TU, so there is no ODR clash. StateManager's
// RTTI-registration (AddToClassTypeInfoArray @ 0x8268DFE8) registers
// ClassTypeInfo<StateManager> descriptors into a separate static array.
template <typename T>
struct ClassTypeInfo
{
    ClassTypeInfo(s32 aiObjectID,
                  const char* apcTypeName,
                  ClassTypeInfo<T>* apBaseTypeInfo,
                  T* (*apfnCreateObject)(u32))
        : ObjectID(aiObjectID)
        , typeName(apcTypeName)
        , baseTypeInfo(apBaseTypeInfo)
        , createObject(apfnCreateObject)
    {
    }

    s32               ObjectID;     // CgsStateManager.h:316
    const char*       typeName;     // CgsStateManager.h:317
    ClassTypeInfo<T>* baseTypeInfo; // CgsStateManager.h:318
    T* (*createObject)(u32);        // CgsStateManager.h:319
};

class StateManager : public CgsSound::MemBase
{
public:
    // CgsStateManager.h:135 (DWARF). Static class-array capacity (matches the X360
    // scan bound 0x10 in AddToClassTypeInfoArray @ 0x8268DFE8).
    static const u16 KU_SIZEOF_CLASS_ARRAY = 16;

    // Returns true when this manager's map-state equals liState. Recovered from
    // the X360 leaf accessor at 0x82680D48 (member access by name; no offset cast).
    virtual bool IsStateAlias(s32 liState) const
    {
        return meMapState == liState;
    }

    // CgsStateManager.h:363 (DWARF) @ 0x8268DFE8. Register a per-class RTTI
    // descriptor into StateManager's static ClassTypeInfo<StateManager> array.
    // Scans for the first empty slot (cap KU_SIZEOF_CLASS_ARRAY == 16); only asserts
    // "Too Many Class registations" once the 16-bit slot counter reaches 4*KU.
    // Returns the registered descriptor.
    static ClassTypeInfo<StateManager>* AddToClassTypeInfoArray(ClassTypeInfo<StateManager>* apTypeInfo);

    // @ 0x8268D798. Returns the address of StateManager's static ClassTypeInfo
    // descriptor (the X360 returns &unk_82F2FAA0; the sibling GetTypeName @0x8268D7A8
    // reads "StateManager" from that descriptor's typeName at +4). Same shape as the
    // EffectControl/EffectObject GetStaticTypeInfo accessors -- a function-local static
    // descriptor seeded with the recovered type name, so the observable return (the
    // descriptor whose typeName is "StateManager") matches.
    static ClassTypeInfo<StateManager>* GetStaticTypeInfo();

protected:
    // Leading scalar members (DWARF CgsStateManager.h:335..341). Modelled by name
    // so meMapState can be read without a raw-offset cast; the trailing keystone
    // members are intentionally omitted (see header FLAG).
    f32 mfCurrentTime;          // CgsStateManager.h:335  (+0x04 on X360)
    f32 mfDeltaTime;            // CgsStateManager.h:336  (+0x08 on X360)
    f32 mfTimeStepGame;         // CgsStateManager.h:338  (+0x0C on X360)
    f32 mfTimeStepSimulation;   // CgsStateManager.h:339  (+0x10 on X360)
    s32 meMapState;             // CgsStateManager.h:341  (+0x14 on X360)
};

}
}

#endif // CGS_SOUND_LOGIC_CGSSTATEMANAGER_H
