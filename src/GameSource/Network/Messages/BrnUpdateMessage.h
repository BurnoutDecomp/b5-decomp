// ===================================================================================
// BrnNetwork::UpdateData -- owning header
//   b5-decomp/src/GameSource/Network/Messages/BrnUpdateMessage.h
//
// SHAPE authoritative from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Messages/BrnUpdateMessage.h:64),
// gated against the X360 binary copy stride / store order:
//   BrnNetwork::UpdateData::operator=   @ 0x82579CD0
//
// LAYOUT (X360-AUTHORITATIVE byte offsets; copy @0x82579CD0 stores in this exact order):
//   +0x00  u16            mu16SentFrame            (lhz/sth 0)
//   +0x02  u16            mu16FramesSinceStart     (lhz/sth 2)
//   +0x10  Matrix44Affine mMatrix                  (64B; 4 vector lvx/stvx, strides 0/0x10/0x20/0x30 off base+0x10)
//   +0x50  Vector3        mLinearVelocity          (16B; vector lvx/stvx at 0x50)
//   +0x60  Vector3        mAngularVelocity         (16B; vector lvx/stvx at 0x60)
//   +0x70  f32            mfSteering               (lfs/stfs 0x70)
//   +0x74  f32            mfAcceleration           (lfs/stfs 0x74)
//   +0x78  f32            mfBraking                (lfs/stfs 0x78)
//   +0x7C  ECameraStatus  meCameraStatus           (lwz/stw 0x7C; s32 enum, committed home)
//   +0x80  s32            meHeadsetStatus          (lwz/stw 0x80; CgsNetwork::ENetworkHeadsetPlayerStatus, un-homed -> s32)
//   +0x84  bool           mbIsBoosting             (lbz/stb 0x84)
//   +0x85  bool           mbIsCrashing             (lbz/stb 0x85)
//   +0x86  bool           mbIsFreeBurnLobby        (lbz/stb 0x86)
//   +0x87  bool           mbIsRoundNumberOdd       (lbz/stb 0x87)
//   +0x88  bool           mbIsEliminated           (lbz/stb 0x88)
//   +0x89  bool           mbSnap                   (lbz/stb 0x89)
//   +0x8A  bool           mbIsInCarSelect          (lbz/stb 0x8A)
//   +0x8B  bool           mbReceivingPlayerCrashedUs (lbz/stb 0x8B)
//
// The 12-byte gap between +0x04 (end of the two u16s) and +0x10 (mMatrix) is natural
// alignment: Matrix44Affine is alignas(16), so it lands at the next 16-byte boundary.
//
// NOTE on the un-homed enum: the DWARF spells meHeadsetStatus as
// CgsNetwork::ENetworkHeadsetPlayerStatus. That enum has no committed shared home (it is
// already modelled as a plain s32 in BrnNetworkModuleInGamePlayerStatusInterface.h for the
// VOIP-status word), so it is modelled here as its underlying 4-byte s32. The X360 copy
// stores a plain word at +0x80. FLAG: replace with the real enum typedef when CgsNetwork's
// headset-status subsystem lands (offset must not move).
#pragma once

#include "types.hpp"                                                              // u16, s32, f32, bool
#include "rw/math/vpu/types.h"                                                    // rw::math::vpu::Matrix44Affine, Vector3
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // BrnNetwork::ECameraStatus (committed home)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h"          // CgsNetwork::Message (committed base)

namespace BrnNetwork
{
    using ::rw::math::vpu::Matrix44Affine;
    using ::rw::math::vpu::Vector3;

    // DWARF BrnUpdateMessage.h:64 -- one car's per-frame network update payload.
    // sizeof == 0x8C (140 bytes) before trailing alignment pad.
    struct UpdateData
    {
        u16            mu16SentFrame;            // +0x00
        u16            mu16FramesSinceStart;     // +0x02
        // (+0x04..+0x0F: alignment pad ahead of the 16-aligned matrix)
        Matrix44Affine mMatrix;                  // +0x10 (64B)
        Vector3        mLinearVelocity;          // +0x50 (16B)
        Vector3        mAngularVelocity;         // +0x60 (16B)
        f32            mfSteering;               // +0x70
        f32            mfAcceleration;           // +0x74
        f32            mfBraking;                // +0x78
        ECameraStatus  meCameraStatus;           // +0x7C (s32 enum)
        s32            meHeadsetStatus;          // +0x80 (CgsNetwork::ENetworkHeadsetPlayerStatus; un-homed -> s32)
        bool           mbIsBoosting;             // +0x84
        bool           mbIsCrashing;             // +0x85
        bool           mbIsFreeBurnLobby;        // +0x86
        bool           mbIsRoundNumberOdd;       // +0x87
        bool           mbIsEliminated;           // +0x88
        bool           mbSnap;                   // +0x89
        bool           mbIsInCarSelect;          // +0x8A
        bool           mbReceivingPlayerCrashedUs; // +0x8B

        // Memberwise copy assignment @ 0x82579CD0 (store-for-store; body in BrnUpdateMessage.cpp).
        UpdateData& operator=(const UpdateData& lOther);
    };

    // DWARF BrnUpdateMessage.h:87 -- one car's per-frame update network message.
    // Adds the embedded UpdateData payload after the 0x20-byte CgsNetwork::Message base.
    // The committed CgsNetwork::Message base is reused BY NAME here -- not forked.
    //
    // LEDGER FUNCTION bodied in this (header) TU (X360 BURNOUT_X360_ARTIST.XEX):
    //   BrnNetwork::UpdateMessage::GetName  @ 0x827DDED8
    //     -> returns the literal "Update Message" (lis/addi a rodata string, blr). No
    //        member or base access. The remaining declared methods (Construct/PrepareForSend/
    //        Release/Destruct/GetPackedMessageSize/GetUpdateData/GetU16FramesSinceStart/
    //        GetU16SentFrame/PackOrUnpack) live in the sibling BrnUpdateMessage.cpp TU and
    //        are declared here for the class shape but NOT bodied in this TU.
    struct UpdateMessage : public CgsNetwork::Message
    {
    public:
        // Sibling-.cpp methods (declared for class shape; NOT bodied in this TU).
        void          Construct();
        void          PrepareForSend(UpdateData* lpUpdateData);
        void          Release();
        void          Destruct();
        virtual s32   GetPackedMessageSize();          // DWARF :- (sibling .cpp)
        const UpdateData* GetUpdateData();             // DWARF :129 (sibling .cpp)
        u16           GetU16FramesSinceStart();         // DWARF :136 (sibling .cpp)
        u16           GetU16SentFrame();                // DWARF :143 (sibling .cpp)

        // LEDGER func @ 0x827DDED8 -- bodied inline below (DWARF BrnUpdateMessage.h:150).
        virtual const char* GetName() const;

    protected:
        virtual CgsNetwork::PackOrUnpackResult PackOrUnpack();

    private:
        UpdateData mUpdateData;   // +0x20 (DWARF :125)
    };

    // BrnNetwork::UpdateMessage::GetName  @ 0x827DDED8
    //   lis/addi a rodata string literal, blr -- no member or base access.
    inline const char* UpdateMessage::GetName() const
    {
        return "Update Message";
    }
} // namespace BrnNetwork
