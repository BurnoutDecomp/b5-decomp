// Tiny embed check: include the VoiceSchema home alongside the sibling homes it
// composes with (CgsCommon Name + CgsRegistry Entity, already pulled in by
// CgsDataStructures.h), and exercise the reconstructed ctor + SetFeatureSchema so
// the gate sees the layout and bodies. Compile-only (cl /c); not shipped.
//
// VoiceSchema::SK_TYPE_NAME is DEFERRED (defined with the EntityFixer<VoiceSchema>
// registration TU), so we provide a local definition here ONLY so this standalone
// embed check has a value to seed mTypeName from. This local definition is private
// to this anonymous-namespace-bearing TU and never ships.
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"

// Local stand-in for the DEFERRED static. Compile-only convenience for this TU.
namespace CgsSound { namespace Playback {
const Name VoiceSchema::SK_TYPE_NAME("VoiceSchema");
} }

namespace
{
void CgsVoiceSchemaEmbedCheck(CgsSound::Playback::FeatureSchema& rFeature)
{
    using CgsSound::Playback::VoiceSchema;
    using CgsSound::Playback::FeatureSchema;

    // Construct a 1-feature voice schema (the flexible array provides slot 0), then
    // install a feature and read back the accumulated totals by name.
    VoiceSchema lSchema(1);
    lSchema.SetFeatureSchema(0, rFeature);

    (void)lSchema.GetFeatureSchemaCount();
    (void)lSchema.GetSlotCount();
    (void)lSchema.GetParameterCount();
    (void)lSchema.GetOutputParamCount();
}
} // namespace
