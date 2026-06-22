// Compile-only embed/layout check for BrnNetwork::UpdateData (TU: class:BrnNetwork::UpdateData).
// Verifies the owning header is self-contained and that the X360-authoritative byte
// offsets / struct size hold under MSVC. Not linked into the game.
#include "GameSource/Network/Messages/BrnUpdateMessage.h"

#include <cstddef>

namespace
{
    using BrnNetwork::UpdateData;

    // Member byte offsets must match the X360 copy @0x82579CD0 store offsets exactly.
    static_assert(offsetof(UpdateData, mu16SentFrame)              == 0x00, "mu16SentFrame @0x00");
    static_assert(offsetof(UpdateData, mu16FramesSinceStart)       == 0x02, "mu16FramesSinceStart @0x02");
    static_assert(offsetof(UpdateData, mMatrix)                    == 0x10, "mMatrix @0x10");
    static_assert(offsetof(UpdateData, mLinearVelocity)            == 0x50, "mLinearVelocity @0x50");
    static_assert(offsetof(UpdateData, mAngularVelocity)           == 0x60, "mAngularVelocity @0x60");
    static_assert(offsetof(UpdateData, mfSteering)                 == 0x70, "mfSteering @0x70");
    static_assert(offsetof(UpdateData, mfAcceleration)             == 0x74, "mfAcceleration @0x74");
    static_assert(offsetof(UpdateData, mfBraking)                  == 0x78, "mfBraking @0x78");
    static_assert(offsetof(UpdateData, meCameraStatus)             == 0x7C, "meCameraStatus @0x7C");
    static_assert(offsetof(UpdateData, meHeadsetStatus)            == 0x80, "meHeadsetStatus @0x80");
    static_assert(offsetof(UpdateData, mbIsBoosting)               == 0x84, "mbIsBoosting @0x84");
    static_assert(offsetof(UpdateData, mbIsCrashing)               == 0x85, "mbIsCrashing @0x85");
    static_assert(offsetof(UpdateData, mbIsFreeBurnLobby)          == 0x86, "mbIsFreeBurnLobby @0x86");
    static_assert(offsetof(UpdateData, mbIsRoundNumberOdd)         == 0x87, "mbIsRoundNumberOdd @0x87");
    static_assert(offsetof(UpdateData, mbIsEliminated)             == 0x88, "mbIsEliminated @0x88");
    static_assert(offsetof(UpdateData, mbSnap)                     == 0x89, "mbSnap @0x89");
    static_assert(offsetof(UpdateData, mbIsInCarSelect)            == 0x8A, "mbIsInCarSelect @0x8A");
    static_assert(offsetof(UpdateData, mbReceivingPlayerCrashedUs) == 0x8B, "mbReceivingPlayerCrashedUs @0x8B");
}
