"""ARTIST replay PreSim state bodies with injected state and observed service boundaries.

The state fixture is not the full ModuleSingleBuffered/base lifecycle or a game run.
Canonical full-module compilation is a separate gate. --truncate-frame is a
temporary extracted-body control, not an old-tree execution.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import REPO,STRSTREAM_CPP,Tree,definition,compile_and_run

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--truncate-frame',action='store_true')
parser.add_argument('--omit-recording',action='store_true')
args=parser.parse_args()
tree=Tree()
header=tree.read('src/GameSource/Replays/BrnReplayModule.h')
fixture=definition(header,'class ReplayModule : public CgsModule::ModuleSingleBuffered')+';'
fixture=fixture.replace('class ReplayModule : public CgsModule::ModuleSingleBuffered','class ReplayModuleFixture')
fixture=fixture.replace('ReplayModule();','ReplayModuleFixture();').replace(' override','').replace('virtual ','')
fixture=fixture.replace('DebugComponent mDebugComponent;','ObservedDebug mDebugComponent;')
source=tree.read('src/GameSource/Replays/BrnReplayModule.cpp')
signatures=('void ReplayModule::WaitForSerialiseJobs()',
    'void ReplayModule::LockSerialisers()', 'void ReplayModule::UnlockSerialisers()',
    'void ReplayModule::ClearSerialisers(', 'void ReplayModule::RenderDebugHUD()',
    'void ReplayModule::SetStatusInterface(', 'bool ReplayModule::WaitForOpenReplayFiles()',
    'void ReplayModule::CloseReplayFiles(', 'bool ReplayModule::WaitForCloseReplayFiles()',
    'void ReplayModule::BeginRestoring(', 'void ReplayModule::StopRecording(',
    'void ReplayModule::StopPlaying(', 'void ReplayModule::UpdateRecording_PreSim(',
    'void ReplayModule::UpdatePlaying_PreSim(', 'void ReplayModule::Update_PreSim(')
constructor_start=source.index('ReplayModule::ReplayModule()')
constructor_body=source.index('{',source.index('mSerialiseJob(nullptr)',constructor_start))
constructor=source[constructor_start:constructor_body]+definition(source[constructor_body:],'{')
bodies=constructor+'\n'+'\n'.join(definition(source,signature) for signature in signatures)
bodies=bodies.replace('ReplayModule::ReplayModule()','ReplayModuleFixture::ReplayModuleFixture()')
bodies=bodies.replace('ReplayModule::','ReplayModuleFixture::')
if args.truncate_frame:
    bodies=bodies.replace('++miCurrentFrame;','miCurrentFrame=static_cast<s32>(miCurrentFrame+1);')
if args.omit_recording:
    bodies=bodies.replace('UpdateRecording_PreSim(lpInput,lpOutput,leUpdateSet);break;','break;',1)
text='namespace BrnReplays {\n'+fixture+'\n'+bodies+'\n}\n'
disk=tree.read('src/GameSource/Replays/Stream/BrnReplayDiskReadStream.cpp')
gpu=tree.read('src/GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.cpp')
writer=tree.read('src/GameSource/Replays/Stream/BrnReplayWriteStream.cpp')
text+='\nnamespace BrnReplays {\n'+'\n'.join((
    definition(disk,'DiskReadStream::DiskReadStream()'),
    definition(disk,'void DiskReadStream::Construct()'),
    definition(gpu,'GPUDiskWriteStream::GPUDiskWriteStream()'),
    definition(writer,'void WriteStream::ResetStartFrame(')))+'\n}\n'
handle=tree.read('src/GameShared/GameClasses/System/FileSystem/CgsFileHandle.cpp')
text+='\nnamespace CgsFileSystem {\n'+definition(handle,'FileHandle& FileHandle::operator=(')+'\n'
text+=definition(handle,'FileState FileHandle::GetStatus() const')+'\n}\n'
job=tree.read('src/SDKs/EATech/eajobs/job.cpp')
text+='\nnamespace EA { namespace Jobs {\n'+'\n'.join(definition(job,signature) for signature in (
    'static EntryPoint MakeDefaultEntryPoint()', 'void Job::Clear()',
    '    Job::Job(const char* lpcName)', '    Job::~Job()\n'))+'\n} }\n'
sdk=REPO/'src/SDKs/EATech/eajobs'
numeric=compile_and_run(Path(__file__).with_name('ReplayPreSimStates.cpp'),
    'replay_presim_states.inc',text,'ReplayPreSimStates',extra_flags='ntdll.lib',extra_sources=[
        REPO/'src/GameSource/Replays/BrnReplayModuleIO.cpp',
        REPO/'src/GameSource/Replays/BrnReplayRequestInterface.cpp',
        REPO/'src/GameSource/Replays/BrnReplayStatusInterface.cpp',
        REPO/'src/GameSource/Replays/BrnReplayBaseSerialiser.cpp',
        REPO/'src/GameSource/Replays/Stream/BrnReplayReadStream.cpp',
        REPO/'src/GameSource/Replays/Stream/BrnReplayStreamHeader.cpp',
        REPO/'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',
        REPO/'src/GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.cpp',
        REPO/'src/GameShared/GameClasses/Memory/CgsLinearMalloc.cpp',
        REPO/'src/GameShared/GameClasses/Gui/CgsGuiEventQueue.cpp',
        REPO/'src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp',
        REPO/'src/GameShared/GameClasses/Core/CgsStringUtils.cpp',STRSTREAM_CPP,
        sdk/'entrypoint.cpp',sdk/'event.cpp',sdk/'bucket_list_node.cpp',sdk/'jobs.cpp'])
if numeric is None: raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_presim_states: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
