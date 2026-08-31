#ifndef BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H

#include "types.hpp"
#include "SharedClasses/Sound/Engines/BrnSoundLoopModelData.h"          // Graph / Partial / Point (BY NAME)
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"        // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoice.h"                // CgsSound::Logic::Voice member (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"         // CgsSound::Logic::VoiceWrapper member (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsContent.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

// =============================================================================
// BrnSound::Vehicles::Engines::DualGinsuEffect  (+ nested LoopInputs / LoopOutputs)
//   GameSource/Sound/Vehicles/Engines/BrnDualGinsuEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// DWARF (BrnDualGinsuEffect.h:150): DualGinsuEffect : public BrnEffectObject. The
// dual-Ginsu (accel + decel grain) engine sound EFFECT OBJECT: it mixes up to
// KI_LOOP_VOICE_COUNT loop-model voices against a partial<->voice mapping.
//
// UPGRADE NOTE: this class was previously modelled as a `namespace DualGinsuEffect`
// hosting only LoopInputs/LoopOutputs (for LoopOutputs::Update / UpdateGainGraph). It
// is now the real X360-attested STRUCT deriving from BrnEffectObject, with LoopInputs /
// LoopOutputs as NESTED types, so the effect's own members + methods (and the
// DualGinsuExhaustEffect subclass) can be homed. The LoopOutputs graph-evaluator bodies
// are preserved verbatim, re-scoped to DualGinsuEffect::LoopOutputs.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer/vptr members on the host.
// =============================================================================

// mapLoopContentSpecs holds pointers to CgsSound::Logic::Content (the per-loop content
// SPEC descriptor). Pointer-only here, so a forward declaration avoids re-homing it.
namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// Pointer-only control back-references (forward-declared to avoid header cascades).
struct HybridExhaustControl;
struct PhysicsControl;
struct EngineControl;

struct DualGinsuEffect : public BrnSound::Logic::BrnEffectObject
{
    // DWARF BrnDualGinsuEffect.h:48/49.
    static const s32 KI_LOOP_VOICE_COUNT = 3;
    static const s32 KI_MAX_LOOPS        = 10;

    // BrnDualGinsuEffect.cpp axis enums (asm-attested via the assert == comparisons).
    // The X-axis (input) selector for a Graph.
    struct LoopInputs
    {
        enum EAxis
        {
            E_UNKNOWN     = 0, // "mi8XAxis != LoopInputs::E_UNKNOWN"
            E_RPM         = 1, // "mi8XAxis == LoopInputs::E_RPM"
            E_ACCELERATOR = 3, // "mi8XAxis == LoopInputs::E_ACCELERATOR"
        };
    };

    // The per-update output accumulator: one f32 per output axis.
    struct LoopOutputs
    {
        // The Y-axis (output) selector; also the index into mafOutputs.
        enum EAxis
        {
            E_UNKNOWN = 0,
            E_GAIN    = 1, // mafOutputs[+4]
            E_PITCH   = 2, // mafOutputs[+8]
            E_COUNT   = 3,
        };

        f32 mafOutputs[E_COUNT]; // stfs at +0/+4/+8

        // @ 0x826B3DD8 -- reset outputs to 1.0, validate the Partial's three graphs,
        // then evaluate the gain graphs + the pitch graph. Returns 1 if the first gain
        // graph produced a non-zero gain.
        s32  Update( f64 aRpm, f64 aAccelerator,
                     const BrnSound::Vehicles::Engines::Partial* apPartial );
        // @ 0x82699F90 -- evaluate one gain Graph and multiply the GAIN output in place.
        bool UpdateGainGraph( f64 afInput,
                              const BrnSound::Vehicles::Engines::Graph* apGraph );
        // (external TU) -- evaluate the pitch graph and drive the PITCH output.
        void UpdatePitchGraph( f64 afInput,
                               const BrnSound::Vehicles::Engines::Graph* apGraph );
    };

    // DWARF BrnDualGinsuEffect.h:52. The prepare sub-state.
    enum EDualGinsuPrepareState
    {
        E_GINSU_PREPARE_STATE_NONE = 0,
        E_GINSU_PREPARE_STATE_INITIALIZE_SUBMIX = 1,
        E_GINSU_PREPARE_STATE_WAIT_FOR_CREATE = 2,
        E_GINSU_PREPARE_STATE_CREATE_VOICES = 3,
        E_GINSU_PREPARE_STATE_VOICES_ARE_PLAYING = 4,
        E_GINSU_PREPARE_STATE_FINISHED = 5,
    };

    DualGinsuEffect();               // @ 0x826C8980
    virtual ~DualGinsuEffect();      // @ 0x826E0EF0 (scalar deleting destructor anchor)

    virtual s32 GetController(s32 aiSlot); // @ 0x82684E88
    virtual void AttachController(CgsSound::Logic::EffectBase* apController); // @ 0x82684EC0
    virtual void SetupLoadData(); // @ 0x826E41D8
    virtual bool Attach(); // @ 0x826F14C0
    virtual void UpdateParams(f32 afTimeStep); // @ 0x826B3770
    virtual void ProcessUpdate(); // @ 0x826FC7C8
    virtual bool Detach(); // @ 0x826EBF00

    // @ 0x826EC090 -- bind partial<->voice both ways, attach content, start the voice.
    void AttachPartialToVoice( s32 liPartial, s32 liVoice );
    // @ 0x826E0F78 -- evict the quietest loop voice under a gain threshold; returns its slot.
    s32  GetFreeLoopModelVoice( f32 afThreshold );
    void DetachPartialFromVoice(s32 aiVoice);
    void UpdateLoopModelParams();
    void UpdateAccelGinsu();
    void UpdateDecelGinsu();

    // ---- members (DWARF order; offsets are X360 facts, not asserted on host) ----
    HybridExhaustControl* mpHybridControl;   // h:102
    PhysicsControl*       mpPhysicsControl;  // h:103
    EngineControl*        mpEngineControl;   // h:104
    EDualGinsuPrepareState meDualGinsuPrepareState; // h:174
    CgsSound::Logic::Voice mLoopModelVoice[KI_LOOP_VOICE_COUNT]; // h:201
    s32  maiVoiceToPartial[KI_LOOP_VOICE_COUNT];  // h:202
    s32  maiPartialToVoice[KI_MAX_LOOPS];         // h:203
    LoopOutputs mLoopModelOutput[KI_MAX_LOOPS];   // h:205
    u32  miNumberOfLoops;                         // h:206
    CgsSound::Logic::Content maLoopContentSpecs[KI_MAX_LOOPS]; // h:208
    const CgsSound::Logic::Content* mapLoopContentSpecs[KI_MAX_LOOPS]; // h:209
    CgsSound::Logic::VoiceWrapper mAccelGinsuVoice;  // h:212
    CgsSound::Logic::VoiceWrapper mDecelGinsuVoice;  // h:213
    CgsSound::Logic::Voice mCarSubmix;
    CgsSound::Logic::Voice mAICarReverbSubmix;
    CgsSound::Logic::Command::QueueElement mSubmixIdent;
    s8 miCarSubmixSend01Index;
    s8 miCarSubmixReverbIndex;
    s8 miCarSubmixPanningAngleIndex;
    s8 miCarSubmixPanningDistanceIndex;
    s8 miCarSubmixPanningSizeIndex;
    s8 miCarSubmixPanningTwistIndex;
    s8 miCarSubmixPanningCentreLevelIndex;
    s8 miCarSubmixPanningMainLevelIndex;
    s8 miCarSubmixPanningLfeLevelIndex;
    s8 miCarSubmixCutoffFreq;
    s8 miCarSubmixLowShelfFreq;
    s8 miCarSubmixLowShelfGain;
    s8 miCarSubmixHighShelfFreq;
    s8 miCarSubmixHighShelfGain;
    s8 miCarSubmixPeakingFreq;
    s8 miCarSubmixPeakingGain;
    s8 miCarSubmixPeakingQ;
    CgsResource::ResourceHandle mLoopModelResource;
    CgsResource::ResourceHandle mAccGinsuResource;
    CgsResource::ResourceHandle mDecGinsuResource;
    CgsResource::ResourceHandle mAttribs;
    CgsSound::Utils::DataPoint<f32> mfAccelGinsuRPM;
    CgsSound::Utils::DataPoint<f32> mfDecelGinsuRPM;
    CgsSound::Utils::DataPoint<f32> mfLoopModelRPM;
    f32 mfMinRpm;
    f32 mfDecelMinRpm;
    f32 mfLowShelfFreq;
    f32 mfLowShelfGain;
    f32 mfHighShelfFreq;
    f32 mfHighShelfGain;
    f32 mfPeakingFreq;
    f32 mfPeakingGain;
    f32 mfPeakingQ;
    s32 miAIEngineIndex;
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_DUAL_GINSU_EFFECT_H
