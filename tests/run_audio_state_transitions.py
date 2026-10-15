"""Exercise stream replacement, completed fades, ready events and ready-content timing.
Pass --rev HEAD for the negative control against the pre-fix production bodies.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, compile_and_run, definition
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
stream = tree.read('src/GameSource/Sound/Global/BrnMusicStream.cpp')
wrapper = tree.read('src/GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.cpp')
requests = tree.read('src/GameSource/Sound/Streaming/BrnStreamingStateManager.cpp')
header = tree.read('src/GameSource/Sound/Global/BrnMusicEffect.h')
text = 'namespace BrnSound { namespace Logic { namespace Streaming {\n'
for sig in ['StreamRequest::StreamRequest(IStreamUser*', 'StreamStopRequest::StreamStopRequest(const IStreamUser*']:
    text += definition(requests, sig) + '\n'
text += '}\n'
for sig in ['MusicStream::MusicStream()', 'void MusicStream::Queue(', 'void MusicStream::Stop(', 'void MusicStream::Update(',
            'void MusicStream::SetSongQueued(', 'const CgsSound::Logic::VoiceWrapper::CreateParams& MusicStream::GetCreateParams() const',
            'void MusicStream::StreamStopped()']:
    text += definition(stream, sig) + '\n'
text += '} }\nnamespace CgsSound { namespace Logic {\n' + definition(wrapper, 'void VoiceWrapper::Update()') + '\n} }\n'
method = 'HasVoiceStartedPlaying' if 'HasVoiceStartedPlaying' in header else 'IsVoiceStageSettledOrPlaying'
result = compile_and_run(Path(__file__).with_name('AudioStateTransitions.cpp'),
                         'audio_state_transitions.inc', text, 'AudioStateTransitions',
                         shadow={'src/GameSource/Sound/Global/BrnMusicEffect.h': header},
                         extra_flags='/DAUDIO_READY_METHOD=' + method)
raise SystemExit(0 if result and result[1] == 0 else 1)
