// ============================================================================
// CgsGenericRwacCommands.cpp -- generic-RWAC command / parameter copy ctors.
//
// Definition home for the fixed-size generic-RWAC command records dispatched
// through GenericRwacFactory::HandlePluginEvent. Bodied from
// BURNOUT_X360_ARTIST.XEX:
//   RwacCommandPluginEvent::ctor                   @ 0x826813B8
//   RwacCommandPluginGetAttribute::ctor            @ 0x826814C8
//   RwacCommandPlayerPlayParameters::ctor          @ 0x826AD578
//   RwacCommandPlayerIsRequestDoneParameters::ctor @ 0x82681570
//   RwacCommandApplyReverbIRFile::ctor             @ 0x826816A0
//   RwacCommandGinsuAttachDataParameters::ctor     @ 0x82681738
//   RwacCommandVoiceCreateInstance::ctor           @ 0x82681170
//   RwacCommandVoiceRelease::ctor                  @ 0x82681320
//   RwacCommandSendConnectParameters::ctor         @ 0x82681608
//   RwacCommandQueue::GetCommand                   @ 0x82681888
//
// Each constructor copy-constructs a command record from a source record of the
// same shape (X360: a chain of lwz N(r5) / stw N(r3) over the record's words),
// then runs two debug asserts:
//   (1) sizeof(*this)/sizeof(uintptr_t) == luCommandCount -- the source-supplied
//       word count matches this record's own word count (2 for the three
//       single-operand commands, 3 for PlayerPlay; X360 cmplwi r4 against 2 / 3);
//   (2) the copied leading tag word equals the class's expected ERwacCommandType
//       (PlayerPlay == 6 / PlayerIsRequestDone == 7 / ReverbIR_apply == 9 /
//       GinsuAttach == 10; X360 cmpwi the just-stored 0(this) against 6/7/9/10).
//
// PlayerPlay additionally AddRefs its first operand: when the play-request
// pointer (word 1) is non-null the X360 ctor does lwz r10,4(r11) / addi r10,r10,1
// / stw r10,4(r11) -- ++mpPlayRequest->mu32RefCount -- BEFORE storing word 2 and
// running the asserts (matching the X360 instruction order).
//
// The asserts are vacuous in the house CGS_ASSERT substitution but are reproduced
// so the validated invariants stay visible. Member access is BY NAME (the tag via
// the RwacCommand base, operands via the named slots); no raw-offset cast.
//
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacCommands.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsSound
{
namespace Playback
{

// ---------------------------------------------------------------------------
// RwacCommandPluginEvent::ctor  @ 0x826813B8
//   build a 4-word record: tag 3 to word 0, the plugin pointer to word 1, and two
//   operands to words 2,3; assert the plugin pointer is non-null.
//
//   X360 store order (r31 == this): stw r4,4(this) [plugin]; stw r11(=3),0(this)
//   [tag]; stw r5,8(this) [op1]; stw r6,0xC(this) [op2]; the plugin non-null guard
//   (cmplwi cr6,r4,0; bne skip) fires the "mpPlugin" assert when arg r4 is zero.
// ---------------------------------------------------------------------------
RwacCommandPluginEvent::RwacCommandPluginEvent(
        uintptr_t apPlugin, uintptr_t auOperand1, uintptr_t auOperand2)
{
    mpPlugin      = apPlugin;                       // stw r4,4(this)
    muCommandType = E_RWAC_COMMAND_PLUGIN_EVENT;    // li r11,3; stw r11,0(this)
    maOperand1    = auOperand1;                      // stw r5,8(this)
    maOperand2    = auOperand2;                      // stw r6,0xC(this)

    CGS_ASSERT(mpPlugin != 0, "mpPlugin");
}

// ---------------------------------------------------------------------------
// RwacCommandPluginGetAttribute::ctor  @ 0x826814C8
//   copy 4 words (tag + 3 operands) from arSource; assert count == 4, tag == 5.
//
//   X360 copies words 0..3 (lwz N(r5)/stw N(r31) for N=0,4,8,0xC) BEFORE the two
//   asserts; the count check (cmplwi cr6,r4,4) precedes the tag check (cmpwi r11,5
//   on the just-stored 0(this)), matching the instruction order here.
// ---------------------------------------------------------------------------
RwacCommandPluginGetAttribute::RwacCommandPluginGetAttribute(
        u32 luCommandCount, const RwacCommandPluginGetAttribute& arSource)
{
    muCommandType = arSource.muCommandType; // lwz r11,0(r5); stw r11,0(r31)
    maOperand1    = arSource.maOperand1;    // lwz r11,4(r5); stw r11,4(r31)
    maOperand2    = arSource.maOperand2;    // lwz r11,8(r5); stw r11,8(r31)
    maOperand3    = arSource.maOperand3;    // lwz r11,0xC(r5); stw r11,0xC(r31)

    CGS_ASSERT(luCommandCount == 4u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE,
               "E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandQueue::GetCommand  @ 0x82681888
//   pop one command word into *apOutWord, advancing the read cursor (wrap & 0xFFF).
//
//   X360 (r28 == this, r27 == apOutWord):
//     lwz r11,0x4000(this) [read]; lwz r10,0x4004(this) [write]; cmplw -- if read
//     == write the ring is empty -> the "Reading whent there's nothing to Read."
//     assert (the original streamed it via StrStream into gpcMessageBuffer; the
//     house CGS_ASSERT forwards the plain string). Then: read the word at
//     queue[read] (lwz r11,0x4000(this); slwi r11,r11,2; lwzx r11,r11,this),
//     store it to *apOutWord (stw r11,0(r27)), and advance read =
//     (read + 1) & 0xFFF (addi r11,r11,1; clrlwi r11,r11,20; stw r11,0x4000(this)).
//   Returns `this` (r3 holds the queue pointer on the normal path).
// ---------------------------------------------------------------------------
RwacCommandQueue* RwacCommandQueue::GetCommand(uintptr_t* apOutWord)
{
    CGS_ASSERT(mu32Read != mu32Write, "Reading whent there's nothing to Read.");

    *apOutWord = mauCommandQueue[mu32Read];
    mu32Read   = (mu32Read + 1u) & 0xFFFu;

    return this;
}

void RwacCommandQueue::PostCommand(uintptr_t auWord)
{
    const u32 luNext = (mu32Write + 1u) & (E_QUEUE_LENGTH - 1u);
    CGS_ASSERT(luNext != mu32Read, "Writing when there's no room to Write.");
    if (luNext == mu32Read)
        return;

    mauCommandQueue[mu32Write] = auWord;
    mu32Write = luNext;
}

// ---------------------------------------------------------------------------
// RwacCommandPlayerPlayParameters::ctor  @ 0x826AD578
//   copy 3 words (tag + 2 operands) from arSource; if operand 0 (the play-request
//   pointer) is non-null, ++mpPlayRequest->mu32RefCount; assert count == 3, tag == 6.
// ---------------------------------------------------------------------------
RwacCommandPlayerPlayParameters::RwacCommandPlayerPlayParameters(
        u32 luCommandCount, const RwacCommandPlayerPlayParameters& arSource)
{
    muCommandType = arSource.muCommandType;  // lwz r11,0(r5); stw r11,0(r31)
    mpPlayRequest = arSource.mpPlayRequest;  // lwz r11,4(r5); stw r11,4(r31)
    if (mpPlayRequest != 0)                  // cmplwi cr6,r11,0; beq
    {
        ++mpPlayRequest->mu32RefCount;       // lwz r10,4(r11); addi r10,r10,1; stw r10,4(r11)
    }
    maOperand = arSource.maOperand;          // lwz r11,8(r5); stw r11,8(r31)

    CGS_ASSERT(luCommandCount == 3u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS,
               "E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandPlayerIsRequestDoneParameters::ctor  @ 0x82681570
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 7.
// ---------------------------------------------------------------------------
RwacCommandPlayerIsRequestDoneParameters::RwacCommandPlayerIsRequestDoneParameters(
        u32 luCommandCount, const RwacCommandPlayerIsRequestDoneParameters& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS,
               "E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandApplyReverbIRFile::ctor  @ 0x826816A0
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 9.
// ---------------------------------------------------------------------------
RwacCommandApplyReverbIRFile::RwacCommandApplyReverbIRFile(
        u32 luCommandCount, const RwacCommandApplyReverbIRFile& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA,
               "E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandGinsuAttachDataParameters::ctor  @ 0x82681738
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 10.
// ---------------------------------------------------------------------------
RwacCommandGinsuAttachDataParameters::RwacCommandGinsuAttachDataParameters(
        u32 luCommandCount, const RwacCommandGinsuAttachDataParameters& arSource)
{
    muCommandType = arSource.muCommandType; // stw r11,0(this)
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS,
               "E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandVoiceCreateInstance::ctor  @ 0x82681170
//   build a 6-word record: the playback voice to word 1, tag 1 to word 0, the
//   voice pointer-slot to word 2, the plugin pointer-slot to word 3, the config to
//   word 4 and a trailing operand to word 5; assert the first four pointers non-null.
//
//   X360 store order (r31 == this): stw r4,4(this) [mpPlaybackVoice]; li r11,1 /
//   stw r11,0(this) [tag]; stw r5,8(this) [mppVoice]; stw r6,0xC(this) [mpppPlugin];
//   stw r7,0x10(this) [mpConfig]; stw r8,0x14(this) [trailing operand]. Then four
//   non-null guards in order (cmplwi cr6,arg,0; bne skip) fire the "mpPlaybackVoice"
//   / "mppVoice" / "mpppPlugin" / "mpConfig" asserts. The 6th word carries no check.
// ---------------------------------------------------------------------------
RwacCommandVoiceCreateInstance::RwacCommandVoiceCreateInstance(
        uintptr_t apPlaybackVoice, uintptr_t appVoice, uintptr_t apppPlugin,
        uintptr_t apConfig, uintptr_t auOperand)
{
    mpPlaybackVoice = apPlaybackVoice;                  // stw r4,4(this)
    muCommandType   = E_RWAC_COMMAND_VOICE_CREATE_INSTANCE; // li r11,1; stw r11,0(this)
    mppVoice        = appVoice;                          // stw r5,8(this)
    mpppPlugin      = apppPlugin;                        // stw r6,0xC(this)
    mpConfig        = apConfig;                          // stw r7,0x10(this)
    maOperand       = auOperand;                         // stw r8,0x14(this)

    CGS_ASSERT(mpPlaybackVoice != 0, "mpPlaybackVoice");
    CGS_ASSERT(mppVoice != 0, "mppVoice");
    CGS_ASSERT(mpppPlugin != 0, "mpppPlugin");
    CGS_ASSERT(mpConfig != 0, "mpConfig");
}

// ---------------------------------------------------------------------------
// RwacCommandVoiceRelease::ctor  @ 0x82681320
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2 and tag == 2.
// ---------------------------------------------------------------------------
RwacCommandVoiceRelease::RwacCommandVoiceRelease(
        u32 luCommandCount, const RwacCommandVoiceRelease& arSource)
{
    muCommandType = arSource.muCommandType; // lwz r11,0(r5); stw r11,0(this)
    maOperand     = arSource.maOperand;     // lwz r11,4(r5); stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_VOICE_RELEASE,
               "E_RWAC_COMMAND_VOICE_RELEASE == GetCommandType()");
}

// ---------------------------------------------------------------------------
// RwacCommandSendConnectParameters::ctor  @ 0x82681608
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2 and tag == 8.
// ---------------------------------------------------------------------------
RwacCommandSendConnectParameters::RwacCommandSendConnectParameters(
        u32 luCommandCount, const RwacCommandSendConnectParameters& arSource)
{
    muCommandType = arSource.muCommandType; // lwz r11,0(r5); stw r11,0(this)
    maOperand     = arSource.maOperand;     // lwz r11,4(r5); stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u, "sizeof(*this) / sizeof(uintptr_t) == luCommandCount");
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS,
               "E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS == GetCommandType()");
}

} // namespace Playback
} // namespace CgsSound
