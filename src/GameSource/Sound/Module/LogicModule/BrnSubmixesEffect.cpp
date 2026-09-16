#include "GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.h"
#include "GameShared/GameClasses/Sound/Logic/CgsDMixDiag.h"   // [DIAG] BRN_DMIX_DIAG witness
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"   // CgsSound::Io::MessageHeader
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"                 // State::GetStateManager()
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"                 // CgsSound::Logic::Voice
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"             // Playback::Name::MakeHash
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"        // Environment::GetAudioMode
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"               // GlobalStateManager::GetSubmixVoice
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"     // SoundLogicModule::GetMasterVoice

// =============================================================================
// BrnSound::Logic::SubmixesEffect -- out-of-line deleting-destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSubmixesEffect.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// ~SubmixesEffect  @ 0x826BC608  (the X360 `scalar deleting destructor')
//   stw  &off_820AE9BC, 0(this)        ; primary vptr settle (SubmixesEffect vtable)
//   stw  &off_820AE988, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle,
//                                        the shared CgsSound::MemBase base vtable)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The `vector deleting destructor'`adjustor{4}' @ 0x826BC600 does `this - 4` (to
// recover the primary object from the IResourceRequester sub-object) and tail-calls
// the scalar deleting destructor above; that sub-object adjustment is the
// compiler-synthesised MI thunk, so it is not reproduced as source.
//
// The dual-vptr settle (+0/+4) and the attach/detach/resources-ready member clears
// (+0x24/+0x28/+0x31) are the inherited BrnEffectObject teardown the compiler emits
// (identical store-for-store to the committed sibling leaves LoopModelEffect
// @ 0x826AFBA0 and SingleGinsuEffect @ 0x826AFAF0); this leaf destructor body adds
// nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

CgsSound::Logic::EffectObject* SubmixesEffect::CreateObject(u32)
{
    // [DIAG] NOT IN THE X360 BINARY (BRN_DMIX_DIAG=1). How many instances of effect
    // type 0x40 the sound data asks for -- one per boot, measured. This line is what
    // established that the Attach/ProcessUpdate lane below is LIVE rather than dead code,
    // back when neither was declared on this leaf and the base's do-nothing versions ran.
    // Kept as the lane's liveness witness.
    if (CgsSound::Diag::DMixDiagEnabled())
    {
        static u32 suCreated = 0;
        CgsSound::Diag::DMixDiagPrintf(
            "[dmix] SubmixesEffect::CreateObject #%u -- effect type 0x40 instantiated\n",
            ++suCreated);
    }
    return new SubmixesEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SubmixesEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x40, "SubmixesEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &SubmixesEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SubmixesEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* SubmixesEffect::GetTypeName() const
{
    return "SubmixesEffect";
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSubmixesEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(SubmixesEffect::GetStaticTypeInfo());

SubmixesEffect::~SubmixesEffect()
{
}

// ---------------------------------------------------------------------------
// SubmixesEffect::Notify  @ 0x82687EE8   (overrides EffectBase::Notify)
//
//   if (message event id @ +0x08 == 15)           ; lhz 8; cmplwi 0xF; bnelr
//       mbHoldVolumes = <payload byte @ +0x10>;    ; lbz 0x10; stb 0x39
//
// BY NAME (2026-08-25 wave 5): the +0x08 halfword is MessageHeader's DWARF
// mi16EventId (read via the new GetEventId accessor) and the +0x10 payload byte is
// Message<bool>::mData -- an event-15 message is the 16-byte header + one bool,
// exactly the committed CgsMessage.h Message<T> shape. The former raw
// reinterpret-index reads are retired.
// ---------------------------------------------------------------------------
void SubmixesEffect::Notify(const CgsSound::Io::MessageHeader* apMessageHeader)
{
    // lhz r11,8(r4); cmplwi 0xF; bnelr -- only event-15 messages are handled.
    if (apMessageHeader->GetEventId() == 15)
    {
        // lbz r11,0x10(r4); stb r11,0x39(r3) -- latch the hold-volumes payload bool.
        mbHoldVolumes =
            static_cast<const CgsSound::Io::Message<bool>*>(apMessageHeader)->mData;
    }
}


// =============================================================================
// The file-scope tuning constants.  The DWARF (BrnSubmixesEffect.cpp:34..54) names all
// thirteen and the X360 lays them out in .rdata in DECLARATION ORDER at
// 0x82F2CE94..0x82F2CEC0, with ONE gap: KF_GLOBAL_LIMITER_THRESHOLD, whose value is 0.0f
// and which therefore lives in .data at 0x82FFB8D4 beside the debug tweakables.
// Corroborated a third time by the debug registrar @0x826D7118, which registers each slot
// under its own label ("Master Gain", "LF Shelf Gain", "LF Shelf Freq", "Limiter Thresh",
// "Limiter Release", "GainArray Centre", "GainArray Front", "Array Rear").
// =============================================================================
static const f32 KF_GLOBAL_GAIN                       =     4.0f;  // .rdata 0x82F2CE94
static const f32 KF_GLOBAL_LOW_SHELF_GAIN             =     0.5f;  // .rdata 0x82F2CE98
static const f32 KF_GLOBAL_LOW_SHELF_FREQ             =    50.0f;  // .rdata 0x82F2CE9C
static const f32 KF_GLOBAL_LOW_SHELF_GAIN_SURROUND    =     0.3f;  // .rdata 0x82F2CEA0
static const f32 KF_GLOBAL_LOW_SHELF_FREQ_SURROUND    =    56.0f;  // .rdata 0x82F2CEA4
static const f32 KF_GLOBAL_LIMITER_THRESHOLD          =     0.0f;  // .data  0x82FFB8D4
static const f32 KF_GLOBAL_LIMITER_RELEASE            =     5.0f;  // .rdata 0x82F2CEA8
static const f32 KF_GLOBAL_LIMITER_GROUPTYPE          =     2.0f;  // .rdata 0x82F2CEAC
static const f32 KF_GLOBAL_GAIN_ARRAY_CENTRE          =     1.0f;  // .rdata 0x82F2CEB0
static const f32 KF_GLOBAL_GAIN_ARRAY_FRONT           =     1.0f;  // .rdata 0x82F2CEB4
static const f32 KF_GLOBAL_GAIN_ARRAY_REAR            =     1.0f;  // .rdata 0x82F2CEB8
static const f32 KF_GLOBAL_GAIN_ARRAY_SUB             =     1.0f;  // .rdata 0x82F2CEBC
static const f32 KF_GLOBAL_GAIN_ARRAY_CENTRE_SURROUND =     1.3f;  // .rdata 0x82F2CEC0

// SubmixesEffect's OWN cutoff clamp, applied on TOP of the one inside the inlined
// EffectBase::GetRWACMixerOutputValue.
static const f32 KF_COLLISION_FILTER_MAX_CUTOFF    = 24900.0f;     // .rdata 0x820B4150
static const f32 KF_COLLISION_FILTER_BYPASS_CUTOFF = 96000.0f;     // .rdata 0x820AA8F0

// =============================================================================
// The auto-generated voice-spec name tables. On the X360 these are dynamically
// initialised .data arrays, each filled by a CRT initialiser that calls
// CgsSound::Playback::Name::MakeHash(<string literal>) @0x82689A50:
//   BrnSound::ParameterIndexes::MasterVoiceSpec::gaMasterVoiceSpecParameterNames
//       @0x83005F50, Name[12], CRT @0x82C63CF8
//   BrnSound::ParameterIndexes::CollisionSubmixVoiceSpec::
//       gaCollisionSubmixVoiceSpecParameterNames @0x83008158, Name[1], CRT @0x82C63CB8
//   BrnSound::SendIndexes::CollisionSubmixVoiceSpec::
//       gaCollisionSubmixVoiceSpecSendNames      @0x8300817C, Name[2], CRT @0x82C635F8
//   BrnSound::SendIndexes::SubmixVoiceSpec::
//       gaSubmixVoiceSpecSendNames               @0x83006000, Name[2], CRT @0x82C63698
// ⭐ NOTHING HERE IS A MAGIC NUMBER. The console stores no constant -- it hashes string
// literals at static-init time, so this tree hashes the SAME literals the same way. The
// literals are the CRT initialisers' own string operands, recovered with
// tools/re/findinit.py; the array cardinalities match the initialisers store-for-store.
// Reproduced as the tree's established file-local KU_ table idiom (cf. BrnScrapeEffect.cpp
// KU_SCRAPE_PARAMETERS, BrnGlobalStateManager.cpp luSend01/luReverbSend) because the
// BrnAutoGen*Indexes headers are not in this tree yet -- see the [FLAG] below.
//
// [FLAG] THE SYMBOLIC INDEX NAMES ARE NOT RECOVERED. The console writes
// `using namespace ParameterIndexes;` (BrnSubmixesEffect.cpp:102) and indexes each array
// with an enumerator. dwarfdump renders those namespaces EMPTY and the enumerators are
// compile-time constants folded into the immediates, so they exist nowhere in the image.
// The NUMBERS below are exact and proven (the literal index equals the array index at all
// 14 call sites); only the symbolic spelling is missing.
// =============================================================================
static const u32 KU_MASTER_VOICE_PARAMETERS[12] =                  // @0x83005F50
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("LowShelfFreq")),       // [0]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("LowShelfGain")),       // [1]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain0")),              // [2]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain1")),              // [3]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain2")),              // [4]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain3")),              // [5]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain4")),              // [6]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain5")),              // [7]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Gain")),               // [8]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("LimiterThreshold")),   // [9]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("LimiterReleaseTime")), // [10]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("LimiterChannelMode")), // [11]
};
static const u32 KU_COLLISION_SUBMIX_PARAMETERS[1] =               // @0x83008158
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("CutoffFreq")),         // [0]
};
static const u32 KU_COLLISION_SUBMIX_SENDS[2] =                    // @0x8300817C
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01")),             // [0]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("ReverbSend")),         // [1] @0x83008180
};
static const u32 KU_SUBMIX_VOICE_SENDS[2] =                        // @0x83006000
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01")),             // [0]
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("ReverbSend")),         // [1] @0x83006004
};

// -----------------------------------------------------------------------------
// SubmixesEffect::Attach  @ 0x826D2DA8   (DWARF BrnSubmixesEffect.cpp:100)
//
// ⭐ Reached through the vtable at 0x820B2530 whose SLOT 0 is the `adjustor{4}' thunk
// @0x826BC600, so it is entered with `this == primary + 4` and every offset below
// carries a +4. See this class's header banner for the full calibration.
//
//   826D2DBC  lhz  r11, 0xE(r31)    -> primary +0x12  mu16AttachCount   \ the INLINED
//   826D2DC0  stw  r29, 0x24(r31)   -> primary +0x28  meDetachState = 0 | EffectBase::
//   826D2DC4  addi r11, r11, 1                                         | Attach()
//   826D2DC8  sth  r11, 0xE(r31)                                       / @0x826A1138
//   826D2DCC  lwz  r11, 8(r31)      -> primary +0x0C  mpState
//   826D2DD8  lwz  r11, 0x24(r11)   -> State  +0x24   mpStateManager  (DWARF CgsState.h:332)
//   826D2DDC  stw  r11, 0x34(r31)   -> primary +0x38  mpStateManager
//   826D2DD0  lwz  r30, 0x28(r31)   -> primary +0x2C  mpLogicModule
//   826D2DD4  addi r28, r30, 0x51F0 -> SoundLogicModule::mMasterVoice  [:111 lMasterVoice]
//   826D2DE0  lwz  r11, 0x2490(r30) + assert "mpObject"               (CgsHandle.h)
//   826D2E10  bl   Environment::GetAudioMode @0x82689BD0 ; cmpwi 1 == E_AUDIO_MODE_SURROUND
//   826D2E20/28  stb 1 or 0, 0x38(r31) -> primary +0x3C  mbIsSurround
//   826D2E30  addi r3, r31, -4      -> recover the PRIMARY this
//   826D2E34  bl   0x826BC6B0       -> UpdateStaticPluginParameters(lMasterVoice)
//   826D2E3C  stb  r29, 0x39(r31)   -> primary +0x3D  mbHoldVolumes = false
//   826D2E38  li   r3, 1            -> return true
//
// [FLAG] The mbHoldVolumes store is SCHEDULED after the helper call. It is unobservable
// -- the helper reads only +0x3C (at 0x826BC6D4 and 0x826BC8A8) and never +0x3D -- so the
// source ORDER is not pinned by the asm; written here in the natural reading.
// -----------------------------------------------------------------------------
bool SubmixesEffect::Attach()
{
    // 826D2DBC..826D2DC8 -- the base's attach bookkeeping, inlined by the console
    // (EffectBase::Attach @0x826A1138 unconditionally returns true, so the compiler
    // folded the `if (!...) return false;` away).
    if (!BrnSound::Logic::BrnEffectObject::Attach())
        return false;

    // 826D2DCC / 826D2DD8 / 826D2DDC
    mpStateManager = static_cast<GlobalStateManager*>(GetStateBase()->GetStateManager());

    // 826D2DD0 / 826D2DD4 -- BrnSubmixesEffect.cpp:111, the DWARF's `lMasterVoice`.
    CgsSound::Logic::Voice& lrMasterVoice =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)->GetMasterVoice();

    // 826D2DE0..826D2E28 -- GetEnvironment() performs the CgsHandle "mpObject" assert
    // itself, so this one call reproduces both the load and the assert the X360 inlines.
    mbIsSurround = (GetLogicModule()->GetPlaybackModule().GetEnvironment()->GetAudioMode() ==
                    CgsSound::Playback::Environment::E_AUDIO_MODE_SURROUND);

    mbHoldVolumes = false;                       // 826D2E3C (see the [FLAG] above)

    // 826D2E30/34 -- `addi r3, r31, -4` recovers the primary object for the private helper.
    UpdateStaticPluginParameters(lrMasterVoice);

    return true;                                 // 826D2E38  li r3, 1
}

// -----------------------------------------------------------------------------
// SubmixesEffect::ProcessUpdate  @ 0x826D2E48   (DWARF BrnSubmixesEffect.cpp:142)
//
// Same +4 base. The four mixer reads are the INLINED EffectBase::GetRWACMixerOutputValue
// @0x82680778: f30 is preloaded with flt_82001CC0 == 0.0f (that function's
// null-mpDynamicMixIo return), each `lwz r3, 0x30(r30)` (primary +0x34, mpDynamicMixIo)
// + `beq` is its null guard, f29 == flt_820AA8F8 == 1/32767 is its DMX_VOL/DMX_DEPTH
// scale, and the `cmpwi r3, 0x6144` / flt_820AA8F0 pair is its DMX_FREQ arm -- all of
// which the committed getter already reproduces, so none of it reappears here.
//
// Locals, from the DWARF (BrnSubmixesEffect.cpp:146-154):
//   lfCollisionReverbSend f27   lCollisionSubmix r31 = mgr+0x98
//   lfCollisionFilterCutOff f31 lPassbySubmix    r29 = mgr+0xA4
//   lfPassbyReverbSend    f28   lMasterVoice     r27 = module+0x51F0
//   lfMasterGain          f30
// -----------------------------------------------------------------------------
void SubmixesEffect::ProcessUpdate()
{
    // 826D2E7C/E80  li r5,4 / li r4,0
    const f32 lfCollisionReverbSend =
        GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_DEPTH);
    // 826D2EBC/EC0  li r5,2 / li r4,1
    f32 lfCollisionFilterCutOff =
        GetRWACMixerOutputValue(1, Nicotine::DMixIO::DMX_FREQ);
    // 826D2F00/F04  li r5,4 / li r4,5
    const f32 lfPassbyReverbSend =
        GetRWACMixerOutputValue(5, Nicotine::DMixIO::DMX_DEPTH);
    // 826D2F38/F3C  li r5,0 / li r4,8
    const f32 lfMasterGain =
        GetRWACMixerOutputValue(8, Nicotine::DMixIO::DMX_VOL);

    // 826D2F74  addi r31, r11, 0x98   (r11 == mpStateManager, cached by Attach)
    CgsSound::Logic::Voice& lrCollisionSubmix =
        mpStateManager->GetSubmixVoice(GlobalStateManager::E_SUBMIX_VOICE_COLLISION);
    // 826D2F88  addi r29, r11, 0xA4
    CgsSound::Logic::Voice& lrPassbySubmix =
        mpStateManager->GetSubmixVoice(GlobalStateManager::E_SUBMIX_VOICE_PASSBY);
    // 826D2F64/F80  lwz r10, 0x28(r30) ; addi r27, r10, 0x51F0
    CgsSound::Logic::Voice& lrMasterVoice =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule)->GetMasterVoice();

    // 826D2F84  lwz r9, 4(0x8300817C) == gaCollisionSubmixVoiceSpecSendNames[1]
    // 826D2F78  li r4, 1 ; 826D2F90  bl 0x826942C0
    lrCollisionSubmix.SetGain(1, lfCollisionReverbSend, &KU_COLLISION_SUBMIX_SENDS[1]);

    // 826D2F98..FA4 -- a SECOND cutoff clamp, on top of the one inside the getter:
    //   lfs f0, flt_820B4150 (24900.0f) ; fcmpu f31, f0 ; ble ; fmr f31, f26 (96000.0f)
    if (lfCollisionFilterCutOff > KF_COLLISION_FILTER_MAX_CUTOFF)
        lfCollisionFilterCutOff = KF_COLLISION_FILTER_BYPASS_CUTOFF;

    // 826D2FB4  lwz r28, dword_83008158 == gaCollisionSubmixVoiceSpecParameterNames[0]
    // 826D2FAC..826D301C -- the console INLINES Logic::Voice::SetParameter here; there is
    // no "liParameterIndex >= 0" assert in this copy because the index is the literal 0
    // and `0 >= 0` folded away.
    lrCollisionSubmix.SetParameter(0, lfCollisionFilterCutOff,
                                   &KU_COLLISION_SUBMIX_PARAMETERS[0]);

    // 826D3038  lwz r11, 4(0x83006000) == gaSubmixVoiceSpecSendNames[1]
    lrPassbySubmix.SetGain(1, lfPassbyReverbSend, &KU_SUBMIX_VOICE_SENDS[1]);

    // 826D3044  lbz r11, 0x39(r30) -> primary +0x3D  mbHoldVolumes
    if (!mbHoldVolumes)
    {
        // 826D3060  lfs f0, flt_82F2CE94 (4.0f) ; 826D3068  fmuls f1, f0, f30
        // 826D3070  lwz r11, 0x20(0x83005F50) == gaMasterVoiceSpecParameterNames[8]
        // 826D3058  li r4, 8 ; 826D3078  bl 0x826AD8C0
        lrMasterVoice.SetParameter(8, KF_GLOBAL_GAIN * lfMasterGain,
                                   &KU_MASTER_VOICE_PARAMETERS[8]);
    }

    // 826D3080  addi r3, r30, -4 ; 826D3084  bl 0x826BC6B0
    UpdateStaticPluginParameters(lrMasterVoice);
}

// -----------------------------------------------------------------------------
// SubmixesEffect::UpdateStaticPluginParameters  @ 0x826BC6B0
//   DWARF BrnSubmixesEffect.cpp:233; the parameter name `lMasterVoice` is the console's
//   own. ⭐ THE FUNCTION NAME IS NOT OURS.
//
// Called by BOTH Attach (0x826D2E34) and ProcessUpdate (0x826D3084) with the PRIMARY this
// (each does `addi r3, rN, -4` first) and the Voice&. Two parameters only; r5 is never read.
//
// ELEVEN SetParameter calls, every index equal to its array index:
//   idx  0  LowShelfFreq        56.0f surround / 50.0f
//   idx  1  LowShelfGain         0.3f surround /  0.5f
//   idx  9  LimiterThreshold     0.0f     idx 10  LimiterReleaseTime  5.0f
//   idx 11  LimiterChannelMode   2.0f
//   idx  3  Gain1 (CENTRE)       1.3f surround /  1.0f
//   idx  2  Gain0 (FRONT L)      1.0f     idx  4  Gain2 (FRONT R)     1.0f
//   idx  5  Gain3 (REAR  L)      1.0f     idx  6  Gain4 (REAR  R)     1.0f
//   idx  7  Gain5 (SUB/LFE)      1.0f
//
// ⭐ WHY THE FOUR GAIN-ARRAY CONSTANTS COVER SIX GAINS: the asm reuses flt_82F2CEB4 for
// indices 2 and 4 and flt_82F2CEB8 for indices 5 and 6. That is an L-C-R-Ls-Rs-LFE channel
// order, which is exactly what the DWARF's CENTRE / FRONT / REAR / SUB names predict --
// an argument independent of both the .rdata ordering and the debug labels, and all three
// agree.
//
// CODEGEN NOTE (no source difference): indices 0, 1 and 3 dispatch the OUT-OF-LINE
// Logic::Voice::SetParameter @0x826AD8C0 while the other eight are inlined. Same statement.
// -----------------------------------------------------------------------------
void SubmixesEffect::UpdateStaticPluginParameters(CgsSound::Logic::Voice& arMasterVoice)
{
    if (mbIsSurround)                                        // 826BC6D4  lbz r11, 0x3C(r28)
    {
        arMasterVoice.SetParameter(0, KF_GLOBAL_LOW_SHELF_FREQ_SURROUND,
                                   &KU_MASTER_VOICE_PARAMETERS[0]);   // 826BC70C/720
        arMasterVoice.SetParameter(1, KF_GLOBAL_LOW_SHELF_GAIN_SURROUND,
                                   &KU_MASTER_VOICE_PARAMETERS[1]);   // 826BC728/740
    }
    else
    {
        arMasterVoice.SetParameter(0, KF_GLOBAL_LOW_SHELF_FREQ,
                                   &KU_MASTER_VOICE_PARAMETERS[0]);   // 826BC6E4/6F8
        arMasterVoice.SetParameter(1, KF_GLOBAL_LOW_SHELF_GAIN,
                                   &KU_MASTER_VOICE_PARAMETERS[1]);   // 826BC700/740
    }

    arMasterVoice.SetParameter(9,  KF_GLOBAL_LIMITER_THRESHOLD,
                               &KU_MASTER_VOICE_PARAMETERS[9]);       // 826BC754/7CC
    arMasterVoice.SetParameter(10, KF_GLOBAL_LIMITER_RELEASE,
                               &KU_MASTER_VOICE_PARAMETERS[10]);      // 826BC7E0/838
    arMasterVoice.SetParameter(11, KF_GLOBAL_LIMITER_GROUPTYPE,
                               &KU_MASTER_VOICE_PARAMETERS[11]);      // 826BC84C/8A4

    if (mbIsSurround)                                        // 826BC8A8  lbz r11, 0x3C(r28)
    {
        arMasterVoice.SetParameter(3, KF_GLOBAL_GAIN_ARRAY_CENTRE_SURROUND,
                                   &KU_MASTER_VOICE_PARAMETERS[3]);   // 826BC8D0/8DC
    }
    else
    {
        arMasterVoice.SetParameter(3, KF_GLOBAL_GAIN_ARRAY_CENTRE,
                                   &KU_MASTER_VOICE_PARAMETERS[3]);   // 826BC8C4/8DC
    }

    // 826BC8F0 / 826BC954 -- the SAME .rdata slot twice (flt_82F2CEB4)
    arMasterVoice.SetParameter(2, KF_GLOBAL_GAIN_ARRAY_FRONT,
                               &KU_MASTER_VOICE_PARAMETERS[2]);       // 826BC948  li r4, 2
    arMasterVoice.SetParameter(4, KF_GLOBAL_GAIN_ARRAY_FRONT,
                               &KU_MASTER_VOICE_PARAMETERS[4]);       // 826BC9B0  li r4, 4
    // 826BC9C4 / 826BCA28 -- the SAME .rdata slot twice (flt_82F2CEB8)
    arMasterVoice.SetParameter(5, KF_GLOBAL_GAIN_ARRAY_REAR,
                               &KU_MASTER_VOICE_PARAMETERS[5]);       // 826BCA1C  li r4, 5
    arMasterVoice.SetParameter(6, KF_GLOBAL_GAIN_ARRAY_REAR,
                               &KU_MASTER_VOICE_PARAMETERS[6]);       // 826BCA84  li r4, 6
    arMasterVoice.SetParameter(7, KF_GLOBAL_GAIN_ARRAY_SUB,
                               &KU_MASTER_VOICE_PARAMETERS[7]);       // 826BCA98/AF0
}

} // namespace Logic
} // namespace BrnSound
