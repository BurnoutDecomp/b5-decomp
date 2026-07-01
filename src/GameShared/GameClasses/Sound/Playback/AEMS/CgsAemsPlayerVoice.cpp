#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsPlayerVoice.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826A2B58
//   CgsSound::Playback::AemsPlayerVoice::GetClientAllocationSize(Factory&, const VoiceSpec&)
//   (DWARF CgsAemsPlayerVoice.h:242, static, returns size_t).
//   called by AemsPlayerVoice::operator new and the AemsPlayerVoice ctor.
//
// X360 behaviour (asm, big-endian):
//   - recover the concrete AemsFactory registry (Factory-4 -> +0x60 == mpRegistry).
//   - key = *VoiceSchema (the schema's first word == its interned Name).
//   - lpCsis = Registry::GetEntity<AemsVoiceCsisClass>(mpRegistry, &key), asserted != 0.
//   - csisCount = lhz *(lpCsis + 0xC)  (u16 parameter count).
//   - fs = VoiceSchema::GetFeatureSchema(GetVoiceSchema(spec), 0) (GetVoiceSchema re-called).
//   - return 4 * ( *(fs + 8) + csisCount + 0x2A )   (paramSchemaCount + csisCount + 42).
//
// The -4/+0x60 AemsFactory recovery and the +0xC/+0x8 field reads are X360 32-bit
// offsets, modelled here BY NAME. FLAG: absolute offsets not asserted (host widths differ).

namespace CgsSound
{
namespace Playback
{
    size_t AemsPlayerVoice::GetClientAllocationSize(Factory& arFactory, const VoiceSpec& arVoiceSpec)
    {
        const Registry* lpRegistry = arFactory.GetAemsRegistry();
        CGS_ASSERT(lpRegistry, "mpRegistry");

        const VoiceSchema* lpSchema = arVoiceSpec.GetVoiceSchema();
        const Name lSchemaName = lpSchema->GetName();
        const AemsVoiceCsisClass* lpCsis = lpRegistry->GetEntity<AemsVoiceCsisClass>(lSchemaName);
        CGS_ASSERT(lpCsis, "lpCsis");

        const u32 lu32CsisParameterCount = lpCsis->GetParameterCount();

        // GetVoiceSchema is re-called (matching the X360 second bl) before GetFeatureSchema(0).
        const FeatureSchema& lrFeatureSchema = arVoiceSpec.GetVoiceSchema()->GetFeatureSchema(0);

        return 4u * (lrFeatureSchema.GetParameterSchemaCount() + lu32CsisParameterCount + 42u);
    }
}
}
