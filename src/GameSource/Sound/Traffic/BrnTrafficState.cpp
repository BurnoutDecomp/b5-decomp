#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficState -- out-of-line bodies.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// The vector deleting destructor: DestroyEffects on the State base, then the compiler's
// vtable settle and the deleting-flavour free (the host operator delete stands in for
// the sound allocator).
// The body is empty: ~State (CgsState.cpp) stores State's vftable and calls DestroyEffects once
// (every State-derived scalar deleting destructor in ARTIST makes exactly that one call, e.g.
// GlobalState 0x826D2278 after storing 0x820AE1F4, State's vftable).
TrafficState::~TrafficState()
{
}

// MemBase::operator new(0x58, "TrafficState", flavour) with the constructor inlined.
CgsSound::Logic::State* TrafficState::CreateObject(u32 luType)
{
    if (luType == 0)
        return new (sTypeInfo.typeName, false) TrafficState();
    else
        return new (sTypeInfo.typeName, true) TrafficState();
}

// Descriptor {0x30000, "TrafficState", BrnState::sTypeInfo, &CreateObject}.
// FLAG: BrnState's own descriptor is not homed, so the base link is null (the
// PassbyState precedent); CreateState only walks the base chain to break a tie between
// two descriptors of the same state and type, and 0x30000 has no rival.
// ARTIST descriptor 0x82F2E8F0.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> TrafficState::sTypeInfo(
    0x30000, "TrafficState", &BrnState::sTypeInfo, &TrafficState::CreateObject);

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* TrafficState::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* TrafficState::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* TrafficState::GetTypeName() const
{
    return sTypeInfo.typeName;
}

// Asserts the attachment, keeps it as the traffic entity (+0x54), then chains to the
// State base Attach.
void TrafficState::Attach( void* lpvAttachment )
{
    CGS_ASSERT_AT( lpvAttachment != 0,
        "lpvAttachment",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
            "../Sound/Vehicles/Traffic/BrnTrafficState.cpp", 66 /* ARTIST 0x826CB29C */);

    mpTrafficEntity = static_cast<const BrnTraffic::BrnTrafficIO::TrafficSoundEntity*>( lpvAttachment );
    State::Attach( lpvAttachment );
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
