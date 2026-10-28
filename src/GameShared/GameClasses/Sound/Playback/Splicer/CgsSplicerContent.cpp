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
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // gpDebugPrint / gxMessageFilterFlags / StrStreamBase

#include <new>

namespace CgsSound
{
namespace Playback
{

SplicerContent::SplicerContent(Factory& arFactory, const ContentSpec& akrSpec,
                               u32 au32Ident)
    : Content(arFactory, akrSpec, au32Ident)
    , mStatistics()
    , mLoader()
    , mpSplicerData(0)
{
}

bool SplicerContent::DoLoad()
{
    return mLoader.Load(*this, GetContentSpec());
}

bool SplicerContent::DoUnload()
{
    return mLoader.Unload(*this, GetContentSpec());
}

void SplicerContent::DoUpdate(f32 /*af32DeltaTime*/)
{
    mLoader.Update(*this, GetContentSpec());
}

void* SplicerContent::DoGetData()
{
    void* lpData = mLoader.GetData();
    CGS_ASSERT(lpData != 0, "lpvData");
    return lpData;
}

// @ 0x826D5AB0. Claim the first of eight bank slots, resolve the endian-mapped
// v1 Splicer image through the owning factory's manager, then attach the bank's
// play-count statistics.
bool SplicerContent::DoOnPostLoad()
{
    void* lpData = Content::GetData(E_CONTENT_STATE_LOADING);
    CGS_ASSERT(mpSplicerData == 0, "0 == mpSplicerData");
    mpSplicerData = lpData;

    SplicerFactory& lrFactory = static_cast<SplicerFactory&>(GetFactory());
    SpliceManager* lpManager = lrFactory.GetManager();
    CGS_ASSERT(lpManager != 0, "mpManager");

    s32 liBank = -1;
    for (s32 li = 0; li < 8; ++li)
    {
        if (lpManager->GetSpliceContainer(static_cast<SPLICE_TYPE>(li)).mpSampleData == 0)
        {
            liBank = li;
            break;
        }
    }
    meType = static_cast<SPLICE_TYPE>(liBank);
    CGS_ASSERT(liBank != -1, "No more Splicer Banks available!");
    if (liBank == -1)
        return false;

    const bool lbLoaded = lpManager->LoadSplice(mpSplicerData, meType);
    CGS_ASSERT(lbLoaded, "Splicer Bank failed to Load");
    if (!lbLoaded)
        return false;

    mStatistics.~SpliceBankStatistics();
    ::new (&mStatistics) SpliceBankStatistics(
        &lpManager->GetSpliceContainer(meType), this, static_cast<u32>(liBank));
    return true;
}

// @ 0x826D5998. Unregister statistics, release the selected bank and clear the
// loaded-data latch before ContentLoader drops the BinaryFileResource.
bool SplicerContent::DoOnPreUnload()
{
    mStatistics.~SpliceBankStatistics();
    ::new (&mStatistics) SpliceBankStatistics();
    CGS_ASSERT(mpSplicerData != 0, "mpSplicerData");

    SplicerFactory& lrFactory = static_cast<SplicerFactory&>(GetFactory());
    SpliceManager* lpManager = lrFactory.GetManager();
    CGS_ASSERT(lpManager != 0, "mpManager");
    lpManager->ClearSplice(meType);
    mpSplicerData = 0;
    return true;
}

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
        *CgsDev::Log::gpDebugPrint << "Name hash: "
                                   << static_cast<u32>(lrName.GetValue()) << "\n"
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
//   if (voice.mpSplice) delete voice.mpSplice;   // the SplicerPlayerVoice member
//   voice.mpSplice = 0;   // unconditional
//   return true;
// The console reads voice+0x88 directly == SplicerPlayerVoice::mpSplice; the
// splicer slot only ever drives splicer player voices, so the down-cast is the
// source-level spelling of that access.
// ---------------------------------------------------------------------------
bool SplicerContentSlot::DoStop(const Slot& /*arSlot*/, PlayerVoice& arVoice, Content& /*arContent*/)
{
    SplicerPlayerVoice& lrVoice = static_cast<SplicerPlayerVoice&>(arVoice);

    if (lrVoice.mpSplice)
        delete lrVoice.mpSplice;   // ~Splice + Splice::operator delete

    lrVoice.mpSplice = 0;          // stored whether or not a splice was present
    return true;
}

// @ 0x826FA648. Count the authored splice, replace the voice's current Splice,
// and start it with the three named Splicer parameters.
bool SplicerContentSlot::DoPlay(const Slot& /*arSlot*/, PlayerVoice& arVoice,
                                Content& arContent, u32 au32Param)
{
    SplicerPlayerVoice& lrVoice = static_cast<SplicerPlayerVoice&>(arVoice);
    SplicerContent& lrContent = static_cast<SplicerContent&>(arContent);
    lrContent.mStatistics.DoPlay(static_cast<u16>(au32Param));
    lrVoice.ForceParameterUpdate();

    if (lrVoice.mpSplice)
    {
        delete lrVoice.mpSplice;
        lrVoice.mpSplice = 0;
    }

    SplicerFactory& lrFactory =
        static_cast<SplicerFactory&>(lrContent.GetFactory());
    SPLICE_Data* lpData = lrFactory.GetManager()->FindSplice(
        lrContent.GetSpliceType(), static_cast<int>(au32Param));
    if (lpData)
        lrVoice.mpSplice = new Splice(lpData, lrVoice.mpInternalSubmix);

    if (lrVoice.mpSplice)
    {
        const f32 lfSpread = lrVoice.GetParameter(
            Name("~SplicerPlayerVoice::Spread~"));
        const f32 lfAzimuth = lrVoice.GetParameter(
            Name("~SplicerPlayerVoice::Azimuth~"));
        const f32 lfPitch = lrVoice.GetParameter(
            Name("~SplicerPlayerVoice::Pitch~"));
        lrVoice.mpSplice->Play(1.0f, lfPitch, lfAzimuth, lfSpread);
    }
    return lrVoice.mpSplice != 0;
}

// @ 0x826E9D58. Advance the active splice and keep the slot playing while any
// of its samples remain live.
bool SplicerContentSlot::DoUpdatePlaying(System* apSystem, const Slot& /*arSlot*/,
                                         PlayerVoice& arVoice,
                                         Content& /*arContent*/, f32 af32Dt)
{
    SplicerPlayerVoice& lrVoice = static_cast<SplicerPlayerVoice&>(arVoice);
    if (!lrVoice.mpSplice)
        return false;

    const f32 lfSpread = lrVoice.GetParameter(
        Name("~SplicerPlayerVoice::Spread~"));
    const f32 lfAzimuth = lrVoice.GetParameter(
        Name("~SplicerPlayerVoice::Azimuth~"));
    const f32 lfPitch = lrVoice.GetParameter(
        Name("~SplicerPlayerVoice::Pitch~"));
    lrVoice.mpSplice->Update(apSystem, 1.0f, lfPitch, lfAzimuth,
                             af32Dt, lfSpread);
    return lrVoice.mpSplice->IsPlaying();
}

// ---------------------------------------------------------------------------
// SplicerContentSlot::DoPreDetach  @ 0x826FA840
//   if (voice.mpSplice) { delete voice.mpSplice; voice.mpSplice = 0; }
// (the store is inside the guard here, unlike DoStop.)
// ---------------------------------------------------------------------------
void SplicerContentSlot::DoPreDetach(const Slot& /*arSlot*/, Voice& arVoice, Content& /*arContent*/)
{
    SplicerPlayerVoice& lrVoice = static_cast<SplicerPlayerVoice&>(arVoice);

    if (lrVoice.mpSplice)
    {
        delete lrVoice.mpSplice;   // ~Splice + Splice::operator delete
        lrVoice.mpSplice = 0;
    }
}

} // namespace Playback
} // namespace CgsSound
