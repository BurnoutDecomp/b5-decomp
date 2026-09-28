#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
TrafficState::~TrafficState()
{
    DestroyEffects();
}

// MemBase::operator new(0x58, "TrafficState", flavour) with the constructor inlined.
CgsSound::Logic::State* TrafficState::CreateObject( u32 /*luType*/ )
{
    return new TrafficState();
}

// Descriptor {0x30000, "TrafficState", BrnState::sTypeInfo, &CreateObject}.
// FLAG: BrnState's own descriptor is not homed, so the base link is null (the
// PassbyState precedent); CreateState only walks the base chain to break a tie between
// two descriptors of the same state and type, and 0x30000 has no rival.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* TrafficState::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State> sTypeInfo(
        0x30000,
        "TrafficState",
        nullptr,
        &TrafficState::CreateObject );
    return &sTypeInfo;
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* const gpTrafficStateReg =
    CgsSound::Logic::State::AddToClassTypeInfoArray( TrafficState::GetStaticTypeInfo() );

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::State>* TrafficState::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* TrafficState::GetTypeName() const
{
    return "TrafficState";
}

// Asserts the attachment, keeps it as the traffic entity (+0x54), then chains to the
// State base Attach.
void TrafficState::Attach( void* lpvAttachment )
{
    CGS_ASSERT( lpvAttachment != 0, "lpvAttachment" );
    mpTrafficEntity = static_cast<const BrnTraffic::BrnTrafficIO::TrafficSoundEntity*>( lpvAttachment );
    State::Attach( lpvAttachment );
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
