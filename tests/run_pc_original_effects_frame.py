"""Original8227FF10 base-frame producer; --rev f74fa5c1 is the missing-body control."""
from pathlib import Path
import argparse
import os
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser()
parser.add_argument('--rev')
args=parser.parse_args()
tree=Tree(args.rev)
effects=tree.read('src/GameSource/Effects/EffectsModule.cpp')
camera=tree.read('src/GameSource/Director/Camera/Camera.cpp')
motion=tree.read('src/GameSource/Director/Camera/BrnCameraEffects.cpp')
depth=tree.read('src/GameSource/Director/Camera/BrnDepthOfField.cpp')
io=tree.read('src/GameSource/Effects/SharedIO/BrnEffectsModuleIO_DispatchInputBuffer.cpp')
parts=['namespace BrnDirector { namespace Camera {',
    definition(camera,'DepthOfField& Camera::GetDepthOfField()'),
    definition(camera,'const DepthOfField& Camera::GetDepthOfField() const'),
    definition(motion,'void MotionBlurData::Set(')]
parts += [definition(depth,signature) for signature in (
    'f32 DepthOfField::GetBlurriness()', 'f32 DepthOfField::GetFocusStartDistanceMeters()',
    'f32 DepthOfField::GetPerfectFocusStartDistanceMeters()',
    'f32 DepthOfField::GetPerfectFocusEndDistanceMeters()', 'f32 DepthOfField::GetFocusEndDistanceMeters()')]
parts += [' } }',
    'namespace BrnEffects { namespace EffectsIO {']
parts += [definition(io,signature) for signature in (
    'BrnEffectsFrame* DispatchInputBuffer::GetBaseEffectsFrame()',
    'const BrnDirector::Camera::Camera* DispatchInputBuffer::GetCameraInput()',
    'void DispatchInputBuffer::SetBaseEffectsFrame(', 'void DispatchInputBuffer::SetCameraInput(')]
parts += ['} }','namespace BrnEffects {',definition(effects,'void EffectsModule::GenerateRenderRequests('),'}']
header=tree.read('src/GameSource/Effects/EffectsModule.h')
cache=definition(header,'struct TempRaceCarStateCache')+';'
result=compile_and_run(Path(__file__).with_name('PCOriginalEffectsFrame.cpp'),
    'original_effects.inc','\n'.join(parts),'PCOriginalEffectsFrame',extra_flags='/Gy /Gw',
    extra_sources=[REPO/'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',STRSTREAM_CPP],
    extra_files={'effects_cache.inc':cache})
raise SystemExit(report('run_pc_original_effects_frame',[],result,1))
