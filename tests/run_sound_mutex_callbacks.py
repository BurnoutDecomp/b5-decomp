"""Execute actual sound lock callbacks; --rev supplies a negative source control."""
from pathlib import Path
import argparse,sys
sys.dont_write_bytecode=True
from fxgs_common import Tree,definition,compile_and_run
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
args=parser.parse_args()
source=Tree(args.rev).read('src/GameSource/Sound/Module/BrnRootSoundModule.cpp')
callbacks='\n'.join(definition(source,signature) for signature in (
    'void RootSoundModule::MutexLockFn()',
    'void RootSoundModule::MutexUnlockFn()',
    'bool RootSoundModule::MutexIsLockedFn()'))
result=compile_and_run(Path(__file__).with_name('SoundMutexCallbacks.cpp'),
                       'sound_mutex_callbacks.inc',callbacks,'SoundMutexCallbacks')
sys.exit(0 if result and not result[1] else 1)
