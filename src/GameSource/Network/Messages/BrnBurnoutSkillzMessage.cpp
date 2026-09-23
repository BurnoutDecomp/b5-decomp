#include "types.hpp"
#include "GameSource/Network/Messages/BrnBurnoutSkillzMessage.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (streamed assert)
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsMessage.h" // PackOrUnpackFloat / PackOrUnpackBool

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BurnoutSkillzMessage::GetPackedMessageSize @ 0x8257C2F0
//   BrnNetwork::BurnoutSkillzMessage::PrepareForSend       @ 0x8257F6C8
//   BrnNetwork::BurnoutSkillzMessage::Retrieve             @ 0x8257F780
// (GetName @ 0x8257C330 is inline in the header.)
//
// A RELIABLE message carrying one player's 56-byte BurnoutSkillzData payload (@+0x28)
// plus an initial-data marker (@+0x60). Its immediate base is ReliableMessage; see the
// header note for the evidence.

namespace BrnNetwork
{
    namespace
    {
        // Skill values are non-negative and unbounded above.
        const f32 KF_MIN_SKILL_VALUE = 0.0f;
        const f32 KF_MAX_SKILL_VALUE = 3.40282347e+38f;
    }

    // The base reliable fields, a cleared skill table and no initial-data marker.
    void BurnoutSkillzMessage::Construct()
    {
        MessageWithPlayerIDs::Construct();
        mSkillzData.Clear();
        mbInitialData = false;
    }

    // The reliable id, then each sent skill at its network accuracy (a negative value is
    // reported before it is packed; the value read back is stored rounded to the same
    // accuracy), then the initial-data marker.
    CgsNetwork::PackOrUnpackResult BurnoutSkillzMessage::PackOrUnpack()
    {
        CgsNetwork::PackOrUnpackResult lxResult = CgsNetwork::ReliableMessage::PackOrUnpack();

        for (BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkillType =
                 BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_START;
             leSkillType < BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_TO_SEND_VIA_NETWORK_COUNT;
             leSkillType++)
        {
            f32 lfSkillValue = mSkillzData.GetBurnoutSkill(leSkillType);
            if (!(lfSkillValue >= KF_MIN_SKILL_VALUE))
            {
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Can't packet value " << lfSkillValue
                        << " for skill type " << static_cast<s32>(leSkillType) << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
            }

            lxResult = CgsNetwork::PackOrUnpackFloat(this, &lfSkillValue, KF_MIN_SKILL_VALUE, KF_MAX_SKILL_VALUE,
                                                     mSkillzData.GetSkillAccuracy(leSkillType)) | lxResult;
            mSkillzData.SetBurnoutSkill(leSkillType, lfSkillValue);
        }

        lxResult = CgsNetwork::PackOrUnpackBool(this, &mbInitialData) | lxResult;
        return lxResult;
    }

    s32 BurnoutSkillzMessage::GetPackedMessageSize()
    {
        mSkillzData.Clear();          // addi r3,r31,0x28 ; bl BurnoutSkillzData::Clear
        mbInitialData = false;        // li r11,0 ; stb r11,0x60(r31)
        return CgsNetwork::ReliableMessage::GetPackedMessageSize();
    }

    void BurnoutSkillzMessage::PrepareForSend(u16 lu16FrameCount,
                                              const BrnGameState::BurnoutSkillzData* lpSkillzData,
                                              bool lbInitialData)
    {
        CGS_ASSERT(lpSkillzData, "lpSkillzData");
        CGS_ASSERT(lu16FrameCount != CgsNetwork::KU16_INVALID_FRAME,
                   "lu16FrameCount != KU16_INVALID_FRAME");

        // A slot already flagged VALID is being re-armed: clear the flag instead of
        // stamping the initial-data marker (asm: lbz 0x19 ; if bit0 set -> clear bit0 ;
        // else stb lbInitialData, 0x60).
        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
            SetMessageInvalid();                   // rlwinm clear bit0 ; stb 0x19
        else
            mbInitialData = lbInitialData;         // stb r28, 0x60(r31)

        mSkillzData = *lpSkillzData;               // memcpy(this+0x28, src, 0x38=56)
        CgsNetwork::ReliableMessage::PrepareForSend(31, lu16FrameCount);
    }

    bool BurnoutSkillzMessage::Retrieve(BrnGameState::BurnoutSkillzData* lpSkillzData,
                                        bool* lpbInitialData)
    {
        CGS_ASSERT(lpSkillzData, "lpSkillzData");
        CGS_ASSERT(lpbInitialData, "lpbInitialData");

        if ((mx8Flags & CgsNetwork::KX8_FLAGS_VALID) != 0)
        {
            *lpSkillzData   = mSkillzData;         // memcpy(dst, this+0x28, 0x38=56)
            *lpbInitialData = mbInitialData;       // lbz 0x60 ; stb 0(r28)
            SetMessageInvalid();                   // clrrwi ; stb 0x19
            return true;
        }

        lpSkillzData->Clear();                     // bl BurnoutSkillzData::Clear
        return false;
    }
} // namespace BrnNetwork
