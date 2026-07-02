#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"

// ============================================================================
// GameSource/Director/Utils/BrnDirectorHookNameArray.cpp
//
// Compilation home for the Array<BrnDirector::HookNameStringWrapper, 100>
// container instantiation (the EffectInterface's 100-slot hook table). The TU is
// the three X360 member instantiations:
//   Array<HookNameStringWrapper,100>::Append              @0x82210418
//   Array<HookNameStringWrapper,100>::Contains            @0x82210540
//   Array<HookNameStringWrapper,100>::FindFirstInstanceOf @0x821FED28
// Each body is the generic template member in CgsArray.h; the anchor below
// forces exactly those three to be emitted, mirroring the X360's export set for
// this element type. Per-instantiation specifics all verified against the asm:
// element stride 33, count word at +0xCE4 (== 100*33), Append's slot assignment
// through the Set-folded HookNameStringWrapper::operator= (@0x821F15B8), and
// FindFirstInstanceOf/Contains comparing through the wrapper's string
// operator== (the inline strcmp loop). The X360's streamed
// "out of space, Length/Capacity" Append text is the committed generic's
// folded static CGS_ASSERT.
//
// Callers in the export set: EffectInterface::Update (Append),
// EffectInterface::HookExists (Contains -> FindFirstInstanceOf).
// ============================================================================

namespace BrnDirector
{

// Out-of-line anchor: exercises exactly the three exported members.
bool HookNameArray_ContainsAnchor(Array<HookNameStringWrapper, 100>& lrArray,
                                  const HookNameStringWrapper& lrName)
{
    lrArray.Append(lrName);
    return lrArray.Contains(lrName);   // -> FindFirstInstanceOf
}

}
