#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"   // SubmixVoice (base)

// CgsSound::Playback::MasterVoice - the single DAC-facing master submix voice
// (a SubmixVoice singleton tracked through spMasterVoice). Class shape / method
// set from the DecFIGS DWARF (CgsMasterVoice.h:39); gated on the X360 ledger.
// This TU bodies the destructor; the constructor and the singleton accessors are
// their own ledger functions (declaration-only here; the accessors are declared
// static per the spMasterVoice singleton they serve -- the DWARF dump does not
// mark staticness).
namespace CgsSound
{
namespace Playback
{
    class Factory;
    struct VoiceSpec;
    enum EVoiceType;   // CgsVoice vocabulary (IsCompatible selector)

    struct MasterVoice : public SubmixVoice
    {
        // DWARF h:78 -- placement construction out of the factory carve.
        MasterVoice(size_t luSize, Factory& lrFactory, const VoiceSpec& lrSpec, u32 lu32Index);

        // @0x826D7A28 (this TU, DWARF cpp:46) -- clear the singleton slot.
        virtual ~MasterVoice();

        // DWARF h:91/h:98/h:106 -- declaration-only (their own ledger functions).
        static bool IsAvailable();
        static MasterVoice& GetMasterVoice();
        bool IsCompatible(EVoiceType leVoiceType);

    private:
        // DWARF cpp:43 -- the singleton slot (X360 dword_82FFB9E0).
        static MasterVoice* spMasterVoice;
    };
}
}
