"""CPU command-bank lifetime, original stall schedule, and producer ownership.

No graphics device is created. Original Im stream operations, shader/effects
banks, IO locks and retained particle records execute against bounded storage.
"""
from pathlib import Path
import argparse
import os
import re
from fxgs_common import Tree, REPO, STRSTREAM_CPP, definition, code_only, compile_and_run, report

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser()
parser.add_argument("--rotate-empty", action="store_true")
parser.add_argument("--read-gui-particles", action="store_true")
parser.add_argument("--old-stall", action="store_true")
parser.add_argument("--old-render-gate", action="store_true")
parser.add_argument("--callback-before-gdl", action="store_true")
parser.add_argument("--repeat-control", action="store_true")
args = parser.parse_args()
tree = Tree()
module = tree.read("src/GameSource/Graphics/BrnRendererModule.cpp")
header = tree.read("src/GameSource/Graphics/BrnRendererModule.h")
methods = "\n".join(definition(module, sig) for sig in (
    "void BrnRendererModule::CompleteCommandFramePC(", "void BrnRendererModule::StartOfFrame()",
    "void BrnRendererModule::SwapBuffers()", "void BrnRendererModule::EndOfFrame(bool"))
for sig, qualified in (
    ("const BrnParticle::ParticleModule::ParticleRenderData* GetPublishedParticleRenderDataPC() const",
     "const BrnParticle::ParticleModule::ParticleRenderData* BrnRendererModule::GetPublishedParticleRenderDataPC() const"),
    ("bool GetPublishedEnvMapFaceRenderPC(u32 luFace) const", "bool BrnRendererModule::GetPublishedEnvMapFaceRenderPC(u32 luFace) const")):
    methods += "\n" + definition(header, sig).replace(sig, qualified)
if args.rotate_empty:
    methods = methods.replace("if (muCompletedCommandGenerationPC != muCommandGenerationPC\n        || muPublishedCommandGenerationPC == muCompletedCommandGenerationPC)", "if (false)")
if args.read_gui_particles:
    methods = methods.replace("lrInput.mbParticleRecordProduced = leProducer == E_COMMAND_PRODUCER_WORLD_AND_EFFECTS;", "lrInput.mbParticleRecordProduced = true;")
if args.old_stall:
    start = methods.index("void BrnRendererModule::EndOfFrame(bool")
    old = definition(methods[start:], "void BrnRendererModule::EndOfFrame(bool")
    methods = methods.replace(old, "void BrnRendererModule::EndOfFrame(bool) { SwapBuffers(); }")
im = tree.read("src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp")
im_methods = "\n".join("template<class V>\n" + definition(im, sig) for sig in (
    "void ImRenderBuffer<V>::Construct()", "void ImRenderBuffer<V>::Clear()", "void ImRenderBuffer<V>::Swap()",
    "const ImCommand* ImRenderBuffer<V>::GetFirstCommand() const"))
im_methods += "\n" + definition(tree.read("src/GameShared/GameClasses/Graphics/CgsCamera.cpp"),
                                 "Camera& Camera::operator=(const Camera& rhs)")
io = tree.read("src/GameSource/Game/BrnDispatchThreadInputBuffer.cpp")
io_methods = "\n".join(definition(io, sig) for sig in (
    "BrnParticle::ParticleModule::ParticleRenderData* DispatchThreadInputBuffer::GetParticleRenderData()",
    "const BrnParticle::ParticleModule::ParticleRenderData* DispatchThreadInputBuffer::GetParticleRenderData() const",
    "void DispatchThreadInputBuffer::SetEnvMapFaceRender("))
io_methods += "\n" + "\n".join(definition(io, sig) for sig in (
    "void DispatchThreadInputBuffer::Construct()", "void DispatchThreadInputBufferManager::Swap()",
    "const BrnParticle::ParticleModule::DispatchThreadUpdateData* DispatchThreadInputBuffer::GetParticleData() const",
    "\n    BrnParticle::ParticleModule::DispatchThreadUpdateData* DispatchThreadInputBuffer::GetParticleData()",
    "const DispatchThreadInputBuffer::CappedInterThreadEventQueue* DispatchThreadInputBuffer::GetParticleInterThreadEventQueue() const",
    "\n    DispatchThreadInputBuffer::CappedInterThreadEventQueue* DispatchThreadInputBuffer::GetParticleInterThreadEventQueue()"))
sig = "const BrnParticle::ParticleModule::ParticleRenderData* DispatchThreadInputBuffer::GetParticleRenderData() const"
real_reader = definition(io_methods, sig)
io_methods = io_methods.replace(real_reader, real_reader.replace("{", "{\n        ++particleReads;", 1))
particles = tree.read("src/GameSource/Effects/Particles/ParticleModule.cpp")
particle_methods = "\n".join(definition(particles, sig).replace("ParticleModule::", "ParticleSnapshotOwner::", 1) for sig in (
    "void ParticleModule::EndOfFrame(bool", "void ParticleModule::PublishRenderCommandsPC("))
ring = definition(tree.read("src/GameShared/GameClasses/Graphics/CgsBufferedDispatchFrame.cpp"),
                  "void BufferedDispatchFrame::Swap()").replace("BufferedDispatchFrame::", "FrameRing::")
effects = definition(tree.read("src/GameSource/Graphics/BrnEffectsArbitrator.cpp"), "void EffectsArbitrator::EndOfFrame()")
defaults = "namespace BrnEffects {\n" + "\n".join(re.findall(r"^const [^;]+;", tree.read(
    "src/SharedClasses/Graphics/BrnEffectsData.cpp"), re.M)) + "\n}"
defaults += "\nnamespace BrnDirector { namespace Camera {\n" + definition(tree.read(
    "src/GameSource/Director/Camera/BrnCameraEffects.cpp"), "void MotionBlurData::Construct()") + "\n} }"
game = tree.read("src/GameSource/Game/BrnGameModule.cpp")
end = code_only(definition(game, "void BrnGameModule::OnEndOfUpdateFrame()"))
render = code_only(definition(module, "void BrnRendererModule::Render("))
render_source = definition(module, "void BrnRendererModule::Render(")
begin_start = render_source.index("    const bool lbDrawFrame =")
begin_if = definition(render_source[begin_start:], "if (!(lbDrawFrame ?")
begin = render_source[begin_start:render_source.index(begin_if, begin_start) + len(begin_if)]
loading_start = render_source.index("if (lpDispatchThreadInputBuffer != 0)")
loading = render_source[loading_start:render_source.index(";", loading_start) + 1]
build_start = render_source.index("    CgsGraphics::DispatchObjectContext lDispatchContext;")
gate = definition(render_source, "if (!lbDrawFrame)")
producer = render_source[build_start:render_source.index(gate, build_start) + len(gate)]
callback_start = producer.index("    {\n        Scope lEffectsProfile(DISPATCH_EFFECTS);")
callback = definition(producer[callback_start:], "    {")
if args.old_render_gate:
    begin = begin.replace("meFrameStallStage == E_FRAMESTALL_NOT_STALLED\n        || meFrameStallStage == E_FRAMESTALL_SYNCING_BUFFERS", "true")
if args.callback_before_gdl:
    producer = producer.replace(callback, "")
    producer = callback + "\n" + producer
if args.repeat_control:
    producer = producer.replace("lpEffectsModule->DispatchThreadUpdate(lpDispatchThreadInputBuffer)",
        "lpEffectsModule->DispatchThreadUpdate(GetRepeatedControlForNegativePC(lpDispatchThreadInputBuffer))")
render_prefix = begin + "\n" + loading + "\n" + producer
effects_callback = definition(tree.read("src/GameSource/Effects/EffectsModule.cpp"),
    "void EffectsModule::DispatchThreadUpdate(").replace("EffectsModule::", "EffectsCallbackOwner::", 1)
dispatch_thread = code_only(definition(game, "void BrnGameModule::DispatchThread()"))
wiring = [
    ("real stall pair reaches both original EOF owners", "const bool lbStalled = mbStalled || mbPrevStalled;" in end
     and "EndOfFrame(lbStalled)" in end and "mbPrevStalled = mbStalled;" in end),
    ("particle snapshots publish only after the shared renderer generation advances", end.index("mRenderModule.EndOfFrame(lbStalled)")
     < end.index("GetPublishedCommandGenerationPC() != luPreviousCommandGeneration") < end.index("PublishRenderCommandsPC(")),
    ("Render no longer privately swaps or clears the seven command banks", "mIm2dDebugRenderBuffer.Swap()" not in render
     and "mIm2dDebugRenderBuffer.Clear()" not in render and "PrepareDebugOverlayForDispatchPC" not in render),
    ("native particle and reflection consumers use retained completed metadata", "lpDispatchThreadInputBuffer->GetParticleRenderData()" not in render
     and "lpDispatchThreadInputBuffer->GetEnvMapFaceRender(" not in render),
    ("original real effects owner reaches Render without a payload-content predicate or outer callback",
     "mRenderModule.Render(&mEffectsModule, lpRead)" in dispatch_thread
     and "GetParticleRenderData" not in dispatch_thread and "DispatchThreadUpdate(" not in dispatch_thread),
]
result = compile_and_run(Path(__file__).with_name("PCCommandFrameLifetime.cpp"), "command_frame_methods.inc", methods,
    "PCCommandFrameLifetime", extra_flags="/Gy /Gw", extra_sources=[REPO / p for p in (
        "src/GameSource/Graphics/BrnShaderConstantsFrame.cpp", "src/GameShared/GameClasses/Module/CgsIOBuffer.cpp",
        "src/GameShared/GameClasses/Graphics/Dispatch/CgsTextureScopeTable.cpp")]
        + [STRSTREAM_CPP], extra_files={"command_im_methods.inc": im_methods, "command_io_methods.inc": io_methods,
        "command_effects_methods.inc": effects, "command_ring_methods.inc": ring,
        "command_effect_defaults.inc": defaults,
        "command_particle_methods.inc": particle_methods,
        "command_render_prefix.inc": render_prefix,
        "command_effects_callback.inc": effects_callback})
raise SystemExit(report("run_pc_command_frame_lifetime", wiring, result, 1))
