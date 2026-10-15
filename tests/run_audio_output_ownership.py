"""Run the actual PC audio backend silently; requires an active Windows endpoint."""
import os
from pathlib import Path
from fxgs_common import REPO, STRSTREAM_CPP, compile_and_run
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
source = (REPO / 'src/GameShared/GameClasses/System/PC/CgsAudioOutputPC.cpp').read_text(encoding='utf-8')
result = compile_and_run(Path(__file__).with_name('AudioOutputOwnership.cpp'),
                         'audio_output_ownership.inc', source, 'AudioOutputOwnership',
                         extra_sources=[STRSTREAM_CPP], extra_flags='ole32.lib uuid.lib')
raise SystemExit(0 if result and result[1] == 0 else 1)
