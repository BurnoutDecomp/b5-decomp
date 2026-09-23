#pragma once

// ===================================================================================
// BrnNetwork::CrashingTrafficMessage -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnCrashingTrafficMessage.h
//
// A CgsNetwork::Message subclass carrying up to KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE
// crashing-traffic records (vehicle id + affine transform). Class shape is taken from
// the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnCrashingTrafficMessage.h)
// and gated on the X360 binary. The base CgsNetwork::Message is the committed home in
// CgsMessage.h and is reused BY NAME here -- not forked.
//
// LEDGER FUNCTION reconstructed in this TU (X360 BURNOUT_X360_ARTIST.XEX):
//   BrnNetwork::CrashingTrafficMessage::GetName  @ 0x827DE088
//     -> returns the literal "Crashing Traffic Message" (lis/addi a rodata string,
//        blr). No member or base access.
//
// The remaining declared methods (Construct/Destruct/PrepareForSend/Retrieve/
// GetPackedMessageSize/GetFramesSinceStart/PackOrUnpack) live in the sibling .cpp TU
// (BrnCrashingTrafficMessage.cpp) and are declared here for the class shape but NOT
// bodied in this TU.
// ===================================================================================

#include "types.hpp"                                                       // s32, u16, bool
#include "rw/math/vpu/types.h"                                             // rw::math::vpu::Matrix44Affine
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"  // CgsNetwork::Message (committed base)

namespace BrnNetwork
{
    using ::rw::math::vpu::Matrix44Affine;

    // DWARF BrnCrashingTrafficMessage.h:35-36 -- record-count bounds.
    const s32 KI_MIN_CRASHING_TRAFFIC_IN_MESSAGE = 0;
    const s32 KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE = 24;

    // DWARF BrnCrashingTrafficMessage.h:50 -- one crashing-traffic record. Member names
    // and logical types are DWARF-derived; project scalar types used.
    struct CrashingTrafficData
    {
        u16             mu16VehicleID;   // DWARF :52
        Matrix44Affine  mMatrix;         // DWARF :53
    };

    // DWARF BrnCrashingTrafficMessage.h:68 -- the network message itself.
    struct CrashingTrafficMessage : public CgsNetwork::Message
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          Destruct();
        void          PrepareForSend(u16 lu16Frame, u16 lu16FramesSinceRoundStart,
                                     s32 liCount, CrashingTrafficData* lpData);
        bool          Retrieve(s32* lpiCount, CrashingTrafficData* lpData);
        s32   GetPackedMessageSize() override;
        // Header-inline on the console (the traffic manager reads the half-word directly).
        u16           GetFramesSinceStart() const { return mu16FramesSinceRoundStart; }

        // LEDGER func @ 0x827DE088 -- bodied in this TU (DWARF BrnCrashingTrafficMessage.h:124).
        const char* GetName() const override;

    protected:
        CgsNetwork::PackOrUnpackResult PackOrUnpack() override;

    private:
        s32                 miCrashingTrafficDataCount;                            // DWARF :106
        u16                 mu16FramesSinceRoundStart;                             // DWARF :107
        CrashingTrafficData maCrashingTrafficData[KI_MAX_CRASHING_TRAFFIC_IN_MESSAGE]; // DWARF :108
    };

    // BrnNetwork::CrashingTrafficMessage::GetName  @ 0x827DE088
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* CrashingTrafficMessage::GetName() const
    {
        return "Crashing Traffic Message";
    }

    // Console size (the RegisterMessageType length), checked on a 32-bit build.
    static_assert(sizeof(void*) != 4 || sizeof(CrashingTrafficMessage) == 0x7B0, "sizeof(CrashingTrafficMessage) == 0x7B0");
} // namespace BrnNetwork
