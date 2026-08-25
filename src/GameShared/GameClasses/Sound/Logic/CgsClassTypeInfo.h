#ifndef CGS_SOUND_LOGIC_CGSCLASSTYPEINFO_H
#define CGS_SOUND_LOGIC_CGSCLASSTYPEINFO_H

#include "types.hpp"

// ============================================================================
// CgsSound::Logic::ClassTypeInfo<T> -- the per-class RTTI descriptor for the
// sound-logic families (EffectBase / State / StateManager). Templated on the leaf
// class; holds the object id, type name, base descriptor and a factory hook.
//
// DWARF: each family header cites the same descriptor shape at its line 313
// (CgsEffectBase.h:313 / CgsState.h:313 / CgsStateManager.h:313 -- the original
// shares one template across the three homes). CANONICAL SINGLE DEFINITION
// (2026-08-25, audio-faithfulness wave 1): the three per-header copies were
// non-identical token sequences (CgsEffectBase.h's carried an extra CreateObject
// convenience member) -- a genuine ODR violation that also broke any TU
// co-including two homes. This header is now the ONE definition; the family
// headers #include it. The CreateObject member is retained (superset; a null-safe
// inline over the createObject hook).
//
// Follows the CgsEnvironment.h "ODR FOLD" precedent (2026-06-25).
// ============================================================================

namespace CgsSound
{
namespace Logic
{

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

    T* CreateObject(u32 au32Param) { return createObject ? createObject(au32Param) : nullptr; }

    s32               ObjectID;     // :316
    const char*       typeName;     // :317
    ClassTypeInfo<T>* baseTypeInfo; // :318
    T* (*createObject)(u32);        // :319
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSCLASSTYPEINFO_H
