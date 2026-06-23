#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.cpp
//
// CgsSound::Playback::SplicerFactory::SplicerAssertFunc -- the splice factory's
// assertion sink. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   CgsSound::Playback::SplicerFactory::SplicerAssertFunc @ 0x8268ABA0
//
// This is the failure path the SplicerFactory routes its assertions through. On
// X360 it builds a message-stream object on the stack (a StrStream over the global
// CgsDev::Assert::gpcMessageBuffer), streams the supplied expression text into it,
// then fires the assert at this file's line 140 (0x8C) via the standard
// Begin/Fire/End sequence:
//
//   CgsDev::Assert::BeginAssert();
//   StrStream s(gpcMessageBuffer);   // off_82000D00 vtable, Clear, seed buffer+config
//   *gpcMessageBuffer = 0;
//   s << (a1 ? a1 : "<NULLSTRING>"); // virtual operator<<(const char*) at vtbl+4
//   CgsDev::Assert::FireAssert(gpcMessageBuffer,
//       "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp",
//       140);
//   return CgsDev::Assert::EndAssert();
//
// The StrStream construct + virtual append is the binary's CgsDev message-streaming
// machinery; the house CGS_ASSERT front-end (CgsAssert.h) forwards a plain string
// to FireAssert instead of streaming it (the streamed form is semantically vacuous
// under the house substitution -- the message that reaches the handler is the same
// expression text). The reconstruction therefore reproduces the observable
// behaviour store-for-store at the assert-front-end boundary: enter the assert,
// fire it with the supplied message (falling back to "<NULLSTRING>" exactly as the
// X360 does when the argument is null), and leave the assert returning EndAssert's
// result. The line number (140) and the null-string fallback are reproduced exactly.
//
// SplicerAssertFunc is itself the assertion -- it always fires (it is the sink the
// factory jumps to once a check has already failed), so there is no condition to
// gate on. The X360 leaves EndAssert's pointer result in r3; that is returned.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsSplicerFactory.h:58 (DWARF). The splice-voice factory. Modelled here only far
// enough to home its assertion sink; the full SplicerFactory (ctor/Create/
// DoCreateVoice/DoCreateContent/DoUpdate/AddRegistry/accessors over mpRegistry,
// mhRwacFactory, mpManager) is DEFERRED to its own TU -- those are engine-gated
// (Registry / GenericRwacFactory / SpliceManager) and outside this slice.
struct SplicerFactory
{
    // Fire the splice factory's assertion with the given
    // expression text. @ 0x8268ABA0. Returns the assert front-end's leave result
    // (the X360 EndAssert pointer), modelled as void* by NAME.
    void* SplicerAssertFunc(const char* lpcExpression);
};

void* SplicerFactory::SplicerAssertFunc(const char* lpcExpression)
{
    // Null-argument fallback, reproduced exactly from the X360 (`<NULLSTRING>`).
    const char* lpcMessage = (lpcExpression != 0) ? lpcExpression : "<NULLSTRING>";

    // Enter the assert, fire it with the (streamed-then-forwarded) message at this
    // file's line 140, and leave -- returning the front-end's leave result. This is
    // the observable behaviour of the X360 Begin/StrStream-append/Fire/End sequence
    // once the StrStream machinery is folded into the house CGS_ASSERT front-end.
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(
        lpcMessage,
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp",
        140);
    return CgsDev::Assert::EndAssert();
}

} // namespace Playback
} // namespace CgsSound
