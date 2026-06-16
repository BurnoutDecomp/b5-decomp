// X360 0x827DB6B0. Compiler-synthesized default constructor (the Feb-2007 leak has no hand-written
// body; the DecFIGS DWARF emits an empty body at BrnStuntManagerDebugComponent.h:50). It exists
// only because members need non-trivial construction: the CgsDev::DebugComponent base (installs its
// vtable, off_820CDE8C as the most-derived) and the three CgsDev::SimpleStrStream subobjects (each
// runs StrStreamBase() -> installs base vtable off_82000D00 + mePrintMode=0, then SimpleStrStream()
// -> installs derived vtable off_82014B00 + resets the inline buffer). The X360 pseudocode is
// exactly that, unrolled (stride 0x108 = 264B per element, 3 elements). mpStuntManager is left
// uninitialised (assigned later by Construct()); the three Vector3 maLastPositions are trivially
// default-constructed. Reusing the real committed base + member types lands the natural layout on
// the X360 offsets (DebugComponent base +0x00..+0x0F, mpStuntManager +0x0C, maStrStreams[0] +0x10
// == r26+0x10), so an explicitly-defaulted ctor regenerates the observed code with no raw-offset
// access and no manual padding.
#include "GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h"

namespace BrnGameState
{
    StuntManagerDebugComponent::StuntManagerDebugComponent() = default;
}
