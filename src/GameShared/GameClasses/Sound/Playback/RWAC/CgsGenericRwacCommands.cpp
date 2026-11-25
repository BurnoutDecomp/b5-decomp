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
// Cited by X360 address only -- no reference-source provenance.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacCommands.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

static const char KAC_ARTIST_ASSERT_FILE[] =
    "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Rwac/CgsGenericRwacFactory.h";

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
    : RwacCommand(arSource.muCommandType)
{
    mpPlugin      = arSource.mpPlugin;      // lwz r11,4(r5); stw r11,4(r31)
    muAttribute   = arSource.muAttribute;   // lwz r11,8(r5); stw r11,8(r31)
    mpfAddress    = arSource.mpfAddress;    // lwz r11,0xC(r5); stw r11,0xC(r31)

    CGS_ASSERT(luCommandCount == 4u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 408 /* ARTIST 0x82681518 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 409 /* ARTIST 0x82681540 */));
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
void RwacCommandQueue::GetCommand(uintptr_t& arWord)
{
    CGS_ASSERT(!IsEmpty(), CGS_ASSERT_STREAM_MESSAGE_AT(
        "Reading whent there's nothing to Read.", CGS_RWAC_FACTORY_H_ASSERT_FILE, 737));
    arWord = mauCommandQueue[mu32Read];
    mu32Read = (mu32Read + 1u) & (E_QUEUE_LENGTH - 1u);
}

// ---------------------------------------------------------------------------
// RwacCommandPlayerPlayParameters::ctor  @ 0x826AD578
//   copy 3 words (tag + 2 operands) from arSource; if operand 0 (the play-request
//   pointer) is non-null, ++mpPlayRequest->mu32RefCount; assert count == 3, tag == 6.
// ---------------------------------------------------------------------------
RwacCommandPlayerPlayParameters::RwacCommandPlayerPlayParameters(
        GenericRwacWaveContent* apWaveContent, f32* apRequestHandle)
    : RwacCommand(E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS)
{
    mpWaveContent = apWaveContent;
    mpRequestHandle = apRequestHandle;
    if (mpWaveContent)
        mpWaveContent->Acquire();
}

RwacCommandPlayerPlayParameters::RwacCommandPlayerPlayParameters(
        u32 luCommandCount, const RwacCommandPlayerPlayParameters& arSource)
    : RwacCommand(arSource.muCommandType)
{
    mpWaveContent = arSource.mpWaveContent;  // lwz r11,4(r5); stw r11,4(r31)
    if (mpWaveContent != 0)
        mpWaveContent->Acquire();            // ++Object::mu32RefCount
    mpRequestHandle = arSource.mpRequestHandle; // lwz/stw word 2

    CGS_ASSERT(luCommandCount == 3u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 458 /* ARTIST 0x826AD5D4 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 459 /* ARTIST 0x826AD5FC */));
}

RwacCommandPlayerPlayParameters::~RwacCommandPlayerPlayParameters()
{
    if (mpWaveContent)
        mpWaveContent->Release();
}

// ---------------------------------------------------------------------------
// RwacCommandPlayerIsRequestDoneParameters::ctor  @ 0x82681570
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 7.
// ---------------------------------------------------------------------------
RwacCommandPlayerIsRequestDoneParameters::RwacCommandPlayerIsRequestDoneParameters(
        u32 luCommandCount, const RwacCommandPlayerIsRequestDoneParameters& arSource)
    : RwacCommand(arSource.muCommandType)
{
    mpParams      = arSource.mpParams;      // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 499 /* ARTIST 0x826815B0 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 500 /* ARTIST 0x826815D8 */));
}

// ---------------------------------------------------------------------------
// RwacCommandApplyReverbIRFile::ctor  @ 0x826816A0
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 9.
// ---------------------------------------------------------------------------
RwacCommandApplyReverbIRFile::RwacCommandApplyReverbIRFile(
        u32 luCommandCount, const RwacCommandApplyReverbIRFile& arSource)
    : RwacCommand(arSource.muCommandType)
{
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 565 /* ARTIST 0x826816E0 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 566 /* ARTIST 0x82681708 */));
}

// ---------------------------------------------------------------------------
// RwacCommandGinsuAttachDataParameters::ctor  @ 0x82681738
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2, tag == 10.
// ---------------------------------------------------------------------------
RwacCommandGinsuAttachDataParameters::RwacCommandGinsuAttachDataParameters(
        void* apRamData)
    : RwacCommand(E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS)
{
    maOperand = reinterpret_cast<uintptr_t>(apRamData);
    CGS_ASSERT(maOperand != 0, "mpvRamData");
}

RwacCommandGinsuAttachDataParameters::RwacCommandGinsuAttachDataParameters(
        u32 luCommandCount, const RwacCommandGinsuAttachDataParameters& arSource)
    : RwacCommand(arSource.muCommandType)
{
    maOperand     = arSource.maOperand;     // stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 599 /* ARTIST 0x82681778 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 600 /* ARTIST 0x826817A0 */));
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
        Voice* lpPlaybackVoice, rw::audio::core::Voice** lppVoice,
        rw::audio::core::PlugIn*** lpppPlugin, GenericRwacVoiceConfig* lpConfig,
        rw::audio::core::PlugIn** lppSubmix)
    : RwacCommand(E_RWAC_COMMAND_VOICE_CREATE_INSTANCE)    // li r11,1; stw r11,0(this)
{
    mpPlaybackVoice = lpPlaybackVoice;                  // stw r4,4(this)
    mppVoice        = lppVoice;                          // stw r5,8(this)
    mpppPlugin      = lpppPlugin;                        // stw r6,0xC(this)
    mpConfig        = lpConfig;                          // stw r7,0x10(this)
    mppSubmix       = lppSubmix;                         // stw r8,0x14(this)

    CGS_ASSERT(mpPlaybackVoice != 0,
        CGS_ASSERT_MESSAGE_AT("mpPlaybackVoice", KAC_ARTIST_ASSERT_FILE, 173 /* ARTIST 0x826811BC */));
    CGS_ASSERT(mppVoice != 0,
        CGS_ASSERT_MESSAGE_AT("mppVoice", KAC_ARTIST_ASSERT_FILE, 174 /* ARTIST 0x826811E4 */));
    CGS_ASSERT(mpppPlugin != 0,
        CGS_ASSERT_MESSAGE_AT("mpppPlugin", KAC_ARTIST_ASSERT_FILE, 175 /* ARTIST 0x8268120C */));
    CGS_ASSERT(mpConfig != 0,
        CGS_ASSERT_MESSAGE_AT("mpConfig", KAC_ARTIST_ASSERT_FILE, 176 /* ARTIST 0x82681234 */));
}

// ---------------------------------------------------------------------------
// RwacCommandVoiceRelease::ctor  @ 0x82681320
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2 and tag == 2.
// ---------------------------------------------------------------------------
RwacCommandVoiceRelease::RwacCommandVoiceRelease(
        u32 luCommandCount, const RwacCommandVoiceRelease& arSource)
    : RwacCommand(arSource.muCommandType)
{
    mpVoice       = arSource.mpVoice;       // lwz r11,4(r5); stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 252 /* ARTIST 0x82681360 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_VOICE_RELEASE,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_VOICE_RELEASE == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 253 /* ARTIST 0x82681388 */));
}

// ---------------------------------------------------------------------------
// RwacCommandSendConnectParameters::ctor  @ 0x82681608
//   copy 2 words (tag + 1 operand) from arSource; assert count == 2 and tag == 8.
// ---------------------------------------------------------------------------
RwacCommandSendConnectParameters::RwacCommandSendConnectParameters(
        u32 luCommandCount, const RwacCommandSendConnectParameters& arSource)
    : RwacCommand(arSource.muCommandType)
{
    maOperand     = arSource.maOperand;     // lwz r11,4(r5); stw r11,4(this)

    CGS_ASSERT(luCommandCount == 2u,
        CGS_ASSERT_MESSAGE_AT(
            "sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
            KAC_ARTIST_ASSERT_FILE, 532 /* ARTIST 0x82681648 */));
    CGS_ASSERT(GetCommandType() == E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS,
        CGS_ASSERT_MESSAGE_AT(
            "E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS == GetCommandType()",
            KAC_ARTIST_ASSERT_FILE, 533 /* ARTIST 0x82681670 */));
}

} // namespace Playback
} // namespace CgsSound
