// Reused sound IO allocations must contain all eleven native replay pointers.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <initializer_list>
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModuleIo.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

static unsigned checks, failures, asserts;
static void Check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++asserts; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; } }
#include "sound_replay_request_lifetime.inc"

using BrnReplays::ReplayIO::RequestInterface;
template<class Buffer> static void CheckBuffer()
{
    struct Guarded {
        u64 before[2];
        Buffer buffer;
        unsigned char after[sizeof(RequestInterface) + 16];
    } storage;
    for (unsigned char poison : {0xA5, 0x5A})
    {
        std::memset(&storage, poison, sizeof(storage));
        unsigned char guard[sizeof(storage.after)];
        std::memcpy(guard, storage.after, sizeof(guard));
        for (unsigned reuse = 0; reuse < 2; ++reuse)
        {
            storage.buffer.Construct();
            storage.buffer.LockForWrite();
            RequestInterface* requests = storage.buffer.GetReplayRequestInterface();
            const auto begin = reinterpret_cast<std::uintptr_t>(&storage.buffer);
            const auto member = reinterpret_cast<std::uintptr_t>(requests);
            const bool contained = member >= begin &&
                member + sizeof(*requests) <= begin + sizeof(Buffer);
            const bool aligned = member % alignof(RequestInterface) == 0;
            Check(contained, "all replay slots belong to this IO allocation");
            Check(aligned, "native replay pointers have their required alignment");
            bool cleared = true;
            for (unsigned slot = 0; slot < BrnReplays::ReplayIO::KI_MAX_SERIALISERS; ++slot)
            {
                std::uintptr_t value;
                std::memcpy(&value, reinterpret_cast<const unsigned char*>(requests)
                            + slot * sizeof(void*), sizeof(value));
                Check(value == 0, "every serialiser slot is reset on reuse");
                cleared = cleared && value == 0;
            }
            if (contained && aligned && cleared)
            {
                RequestInterface source = {};
                auto* serialiser = reinterpret_cast<BrnReplays::BaseSerialiser*>(storage.before);
                source.mapSerialisers[10] = serialiser;
                requests->Append(&source);
                Check(requests->mapSerialisers[10] == serialiser,
                      "last-slot merge preserves the complete native pointer");
            }
            storage.buffer.UnlockForWrite();
            Check(std::memcmp(guard, storage.after, sizeof(guard)) == 0,
                  "construction and merge preserve the following IO allocation");
        }
    }
}
int main()
{
    CheckBuffer<BrnSound::Module::Io::RootOutputBuffer>();
    CheckBuffer<BrnSound::Module::Io::LogicOutputBuffer>();
    Check(asserts == 0, "fresh registrations do not trigger duplicate assertions");
    std::printf("SoundReplayRequestLifetime: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
