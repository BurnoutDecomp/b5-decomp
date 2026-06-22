#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"

// Exercise CgsSound::Io::MessageHeader::Destruct @ 0x82689868.
namespace
{
void ExerciseDestruct(CgsSound::Io::MessageHeader& lHeader)
{
    CgsSound::Io::MessageHeader* lpResult = lHeader.Destruct();
    (void)lpResult;
}
}

// The two reset fields must sit at +0x08 and +0x0A (proven by the asm halfword
// stores). We do not static_assert absolute member offsets, but we can assert the
// header is exactly the proven size (8 leading opaque bytes + two u16 = 12 bytes).
static_assert(sizeof(CgsSound::Io::MessageHeader) == 12,
              "MessageHeader header block must be 12 bytes (qword + 2 x u16)");
