#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CgsDev::Assert::Begin/Fire/EndAssert (explicit, baked file/line)

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the ChainableMultiplierInfo leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template BrnGameState::GameStateModuleIO::ChainableMultiplierInfo* Array<BrnGameState::GameStateModuleIO::ChainableMultiplierInfo, 8>::QSort(int (*)(const void*, const void*));

// ============================================================================
// X360 BrnGameState::GameStateModuleIO::ChainableMultiplierInfo,8>::Ge @ 0x823178F8
//   == Array<ChainableMultiplierInfo, 8>::GetCount() const.
//   The ledger name is truncated to "::Ge"; the body returns *(this + 0x80) ==
//   miCount (NOT an element, and there is NO index argument), so it is the count
//   accessor GetCount(), not the element accessor Ge(u32). miCount lives at +0x80
//   == 8 elements * 16-byte ChainableMultiplierInfo stride. Called by
//   BrnGameState::ScoringSystemDebugComponent::GetChainableTableEntry. Pairs with
//   the already-committed QSort sibling @ 0x82317950.
//
// This is emitted as a FULL MEMBER SPECIALIZATION (not a plain explicit
// instantiation) on purpose: the generic inline Array<T,N>::GetCount() in
// CgsArray.h deliberately has NO assert, because the two committed count getters
// that wrap it -- GameModeParams::GetCheckpointCount / GetStartLocationCount
// (BrnGameModeParams.cpp) -- supply the use-before-Construct assert themselves and
// assert exactly ONCE. Adding the assert to the shared inline GetCount() would make
// those wrappers double-assert (assert inside GetCount() during the
// `== KI_UNCONSTRUCTED` check, then again in the wrapper body) and diverge from the
// verified X360 single-assert shape. So the inline GetCount() is left untouched and
// this one instantiation gets its own asserting body via specialization.
//
// The assert is written with CGS_ASSERT (the project convention); the macro fills
// __FILE__/__LINE__ at this reconstruction rather than reproducing the X360's baked-in
// CgsArray.h:336 - the accepted trade-off, matching the GameModeParams::GetCheckpointCount/
// GetStartLocationCount precedent.
template<>
s32 Array<BrnGameState::GameStateModuleIO::ChainableMultiplierInfo, 8>::GetCount() const
{
    // X360: the unconstructed sentinel is *(a1 + 128) == -1.
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    return miCount;                             // X360: return *(a1 + 128)
}
