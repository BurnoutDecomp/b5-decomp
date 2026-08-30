// ============================================================================
// CgsCommandQueue.cpp
//
// Definition home for CgsSound::Playback::AemsPlayerVoice::Stop
//   reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826DAF10,
// plus the out-of-line template bodies for CommandQueue<>::GetCommand /
// PostCommand (the X360 ARTIST build emitted these out-of-line for the CSIS
// command family; the generic method bodies are DECLARED in CgsCommandQueue.h
// and DEFINED here, followed by the explicit CommandQueue<255u,uintptr_t>
// instantiation the CsisCommandQueue uses).
//
// Stop tears down the voice's outstanding CSIS request: if a request handle is
// live, it posts a CsisReleaseCommand carrying that handle onto the owning
// factory's CsisCommandQueue, clears the handle, and reports that it stopped.
//
// Provenance reconciled against the PS3 DecFIGS body
// (._ZN8CgsSound8Playback15AemsPlayerVoice4StopEv @ PS3 0x85C5E8): the function is
// inlined-defined in CgsAemsFactory.h (PS3 FireAssert cites
// ".../Sound/Playback/Aems/CgsAemsFactory.h":248 for the "Possibly need to increase
// CsisCommandQueue::E_QUEUE_LENGTH" overflow assert) -- CgsCommandQueue.cpp is a
// home of convenience for the queue family, not the source home. The PS3 confirms
// E_CSIS_COMMAND_RELEASE == 2 (the `{2, handle}` command pair pushed) and the
// 0xFF-modulo ring wraparound; the abstracted operator<< below stands in for the
// inlined PostCommand ring push (file home cited per project policy; not moved).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsCommandQueue.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"  // the SINGLE AemsPlayerVoice (Stop bodied below)

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstdint> // std::uintptr_t

namespace CgsSound
{
namespace Playback
{

// CgsCommandQueue.h:117 (X360 0x826A78D0). Pop one command from the ring: the first
// popped word is the payload count; that many payload words are then popped into
// apWords, but no more than the caller's capacity (the incoming arCount). arCount is
// overwritten with the popped payload count. Returns false if the ring underflows
// mid-pop (the read cursor is then rolled back to where it started).
template <unsigned int QUEUE_LENGTH, typename T>
bool CommandQueue<QUEUE_LENGTH, T>::GetCommand(u32& arCount, T* apWords)
{
    CGS_ASSERT(apWords != nullptr, "lpuFirstCommmandOut");

    const u32 luSavedRead = mu32Read; // rolled back on underflow
    const u32 luCapacity  = arCount;  // caller's max payload words

    bool lbOk;
    if (mu32Read == mu32Write)
    {
        lbOk = false;
    }
    else
    {
        lbOk     = true;
        arCount  = mauCommandQueue[mu32Read];
        mu32Read = (mu32Read + 1) % QUEUE_LENGTH;
    }

    if (lbOk && luCapacity >= arCount)
    {
        T* lpOut = apWords;
        for (u32 luIndex = 0u; luIndex < arCount; ++luIndex, ++lpOut)
        {
            if (mu32Read == mu32Write)
            {
                lbOk = false;
            }
            else
            {
                lbOk     = true;
                *lpOut   = mauCommandQueue[mu32Read];
                mu32Read = (mu32Read + 1) % QUEUE_LENGTH;
            }

            if (!lbOk)
                break;
        }
    }

    if (!lbOk)
        mu32Read = luSavedRead;

    return lbOk;
}

// CgsCommandQueue.h:89 (X360 0x826AAAB8). Post a multi-word command: the count word
// auCount is written first, then auCount payload words from apWords. Advancing the
// write cursor onto the read cursor means the ring is full: PostCommand fails and the
// write cursor is rolled back to where it started, discarding the partial command.
template <unsigned int QUEUE_LENGTH, typename T>
bool CommandQueue<QUEUE_LENGTH, T>::PostCommand(unsigned int auCount, const T* apWords)
{
    CGS_ASSERT(apWords != nullptr, "lpuFirstCommand");

    const u32 luSavedWrite = mu32Write; // rolled back on overflow

    // Push the count word.
    mauCommandQueue[mu32Write] = auCount;
    mu32Write = (mu32Write + 1) % QUEUE_LENGTH;
    bool lbOk = (mu32Read != mu32Write); // false once the write cursor catches the read

    if (lbOk)
    {
        const T* lpIn = apWords;
        for (u32 luIndex = 0u; luIndex < auCount; ++luIndex)
        {
            mauCommandQueue[mu32Write] = *lpIn++;
            mu32Write = (mu32Write + 1) % QUEUE_LENGTH;
            lbOk = (mu32Read != mu32Write);

            if (!lbOk)
                break;
        }
    }

    if (!lbOk)
        mu32Write = luSavedWrite;

    return lbOk;
}

// Explicit instantiation of the queue actually used by the CSIS command family
// (CsisCommandQueue @ CgsAemsFactory.h:265): the 255-deep uintptr_t ring. Emits the
// out-of-line GetCommand / PostCommand bodies above.
template struct CommandQueue<255u, std::uintptr_t>;

// AemsPlayerVoice is the SINGLE merged definition in AEMS/CgsAemsPlayerVoice.h
// (2026-08-25 wave 5; the rival 2-member TU-local class that used to live here --
// a hard ODR conflict with the AEMS home -- is retired). Stop stays BODIED here,
// where the queue/command types are complete. The X360 queue route
// (`*(this+8) - 4 + 0x68` == the owning AemsFactory's embedded CsisCommandQueue)
// is documented on the class's mpCommandQueue stand-in.

CsisCommandQueue& operator<<(CsisCommandQueue& arQueue,
                             const CsisReleaseCommand& arCommand)
{
    const bool lbPosted = arQueue.mQueue.PostCommand(
        static_cast<unsigned int>(sizeof(arCommand) / sizeof(std::uintptr_t)),
        arCommand.AsCommand());
    CGS_ASSERT(lbPosted, "Possibly need to increase CsisCommandQueue::E_QUEUE_LENGTH");
    return arQueue;
}

bool CsisCommandQueue::GetCommand(u32& arCount, std::uintptr_t* apWords)
{
    return mQueue.GetCommand(arCount, apWords);
}

} // namespace Playback
} // namespace CgsSound
