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
// header is exactly the proven size: 8 leading opaque bytes (X360 vtable slot +
// meEffectType) + four u16 destination fields = 16 bytes. The 16-byte size is
// X360-attested by the VariableEventQueue<*,16>::AddEvent<Message<T>> record-size
// args (sizeof(Message<bool>) == 0x14 == 16 + 4-byte payload slot; a 12-byte header
// would yield 0x10) and by SubmixesEffect::Notify reading a payload byte at +0x10.
static_assert(sizeof(CgsSound::Io::MessageHeader) == 16,
              "MessageHeader block must be 16 bytes (8 opaque + 4 x u16)");

// A representative Message<T> instantiation must be exactly header + payload.
static_assert(sizeof(CgsSound::Io::Message<bool>) == 20,
              "sizeof(Message<bool>) == 16-byte header + bool (padded to 20)");
static_assert(sizeof(CgsSound::Io::Message<float>) == 20,
              "sizeof(Message<float>) == 16-byte header + float");
static_assert(sizeof(CgsSound::Io::Message<unsigned long long>) == 24,
              "sizeof(Message<unsigned __int64>) == 16-byte header + u64");
