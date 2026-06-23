// ============================================================================
// CgsCommandQueue.cpp
//
// Definition home for CgsSound::Playback::AemsPlayerVoice::Stop
//   reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DAF10.
//
// Stop tears down the voice's outstanding CSIS request: if a request handle is
// live, it posts a CsisReleaseCommand carrying that handle onto the owning
// factory's CsisCommandQueue, clears the handle, and reports that it stopped.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsCommandQueue.h"

#include <cstdint> // std::uintptr_t

namespace CgsSound
{
namespace Playback
{

// Minimal modelled view of AemsPlayerVoice for the fields Stop touches BY NAME.
// (The full type -- AemsPlayerVoice : GenericRwacVoice -- and its absolute layout
// are reconstructed in CgsAemsPlayerVoice.h's own TU. Here only the request handle
// and the by-name route to the command queue are modelled. The X360 reaches the
// queue from the voice's RWAC base via `*(this+8) - 4 + 104`; that raw-offset
// downcast is replaced by a named queue pointer per the project's by-name rule.)
struct AemsPlayerVoice
{
    std::uintptr_t mhRequestHandle; // +152 (X360 +0x98): live CSIS request, or 0
    CsisCommandQueue* mpCommandQueue; // by-name route to the factory's queue

    // 0x826DAF10.
    bool Stop();
};

// 0x826DAF10. If no request is outstanding, report "nothing stopped". Otherwise
// build a release command from the handle, post it to the command queue, clear the
// handle, and report that the voice was stopped.
bool AemsPlayerVoice::Stop()
{
    if (mhRequestHandle == 0)
        return false;

    CsisReleaseCommand lCommand(mhRequestHandle);
    (*mpCommandQueue) << lCommand;

    mhRequestHandle = 0;
    return true;
}

} // namespace Playback
} // namespace CgsSound
