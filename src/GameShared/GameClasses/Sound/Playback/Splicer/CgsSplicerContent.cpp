// ============================================================================
// CgsSplicerContent.cpp -- CgsSound::Playback splicer TTY dumps + slot teardown.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   SpliceBankStatistics::DumpToTty        @ 0x826A2F78
//   SpliceBankStatistics::DumpAllToTty     @ 0x826A30E8
//   SplicerContentSlot::DoStop             @ 0x826FA7E8
//   SplicerContentSlot::DoPreDetach        @ 0x826FA840
//
// DumpToTty / DumpAllToTty are debug-TTY dumps of the per-splice-bank play counts,
// routed through the engine's debug-print stream (CgsDev::Log::gpDebugPrint) and gated
// on the message filter (CgsDev::Message::gxMessageFilterFlags & GLOBAL). DoStop /
// DoPreDetach are the splicer slot's teardown: they delete the per-voice Splice object
// (voice +0x88) and null the pointer.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // gpDebugPrint / gxMessageFilterFlags / StrStreamBase

namespace CgsSound
{
namespace Playback
{

// The Name stream inserter (X360 `CgsSound::Playback::operator<<`, called by DumpToTty
// to render a content name). Its body lives in its own (not-yet-done) TU; declared here
// as a cross-TU callee and resolved at link/consolidation -- NOT defined.
CgsDev::StrStreamBase& operator<<(CgsDev::StrStreamBase& arStream, const Name& arName);

// ---------------------------------------------------------------------------
// SpliceBankStatistics::DumpToTty  @ 0x826A2F78
//
// If this bank has a content and a Stats array: (filtered) print its name and splice
// count, then (per splice, re-checking the filter) print the splice index + play count.
// ---------------------------------------------------------------------------
void SpliceBankStatistics::DumpToTty() const
{
    if (!mpSpliceContent || !mpaStats)
        return;

    if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
    {
        const Name& lrName = mpSpliceContent->GetContentSpec().GetName();
        *CgsDev::Log::gpDebugPrint << "Name: " << lrName << "\n"
                                   << "SpliceCount: " << muStatCount << "\n";
    }

    for (u32 luSplice = 0; luSplice < muStatCount; ++luSplice)
    {
        // The X360 re-tests the filter each iteration (the whole loop still runs, only
        // the print is gated); preserved.
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "SpliceIndex: " << luSplice << "\t"
                                       << "PlayCount: " << mpaStats[luSplice].muPlayCount << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// SpliceBankStatistics::DumpAllToTty  @ 0x826A30E8
//   Walk the intrusive spHead list, dumping each registered bank.
// ---------------------------------------------------------------------------
void SpliceBankStatistics::DumpAllToTty()
{
    for (SpliceBankStatistics* lpBank = spHead; lpBank; lpBank = lpBank->mpNext)
        lpBank->DumpToTty();
}

// ---------------------------------------------------------------------------
// SplicerContentSlot::DoStop  @ 0x826FA7E8
//   if (voice.mpSplice) delete voice.mpSplice;
//   voice.mpSplice = 0;   // unconditional
//   return true;
// ---------------------------------------------------------------------------
bool SplicerContentSlot::DoStop(const Slot& /*arSlot*/, PlayerVoice& arVoice, Content& /*arContent*/)
{
    if (arVoice.mpSplice)
        delete arVoice.mpSplice;   // ~Splice + Splice::operator delete

    arVoice.mpSplice = 0;          // stored whether or not a splice was present
    return true;
}

// ---------------------------------------------------------------------------
// SplicerContentSlot::DoPreDetach  @ 0x826FA840
//   if (voice.mpSplice) { delete voice.mpSplice; voice.mpSplice = 0; }
// (the store is inside the guard here, unlike DoStop.)
// ---------------------------------------------------------------------------
void SplicerContentSlot::DoPreDetach(const Slot& /*arSlot*/, Voice& arVoice, Content& /*arContent*/)
{
    if (arVoice.mpSplice)
    {
        delete arVoice.mpSplice;   // ~Splice + Splice::operator delete
        arVoice.mpSplice = 0;
    }
}

} // namespace Playback
} // namespace CgsSound
