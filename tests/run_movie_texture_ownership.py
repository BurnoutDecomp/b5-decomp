"""CPU movie owner path. Codec EOF, GPU I/O and unrelated acquire/audio arms are observed.
--old-release reproduces the previous destruction before queued consumption.
"""
import argparse, os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, REPO
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument("--old-release",action="store_true")
p.add_argument("--old-keep",action="store_true")
p.add_argument("--old-initial-state",action="store_true")
p.add_argument("--old-rectangle",action="store_true")
a=p.parse_args(); tree=Tree()
root="src/GameShared/GameClasses/Graphics/"
player=tree.read(root+"MoviePlayer/CgsMoviePlayer.cpp")
player_bodies="\n".join(definition(player, key) for key in (
 "void MoviePlayer::Construct()", "void MoviePlayer::ReleaseResources()",
 "bool MoviePlayer::Release()", "void MoviePlayer::RetireArenaTexturesPC()",
 "void MoviePlayer::Stop()", "void MoviePlayer::Update()", "void MoviePlayer::Render(",
 "bool MoviePlayer::EnsureTexture(", "f32 MoviePlayer::ComputeCrossfadeAlpha(",
 "void MoviePlayer::SetRectangle("))
if a.old_initial_state:
 needle="mePlayerState = E_RW_MOVIE_PLAYER_NULL;"
 assert needle in definition(player,"void MoviePlayer::Construct()")
 player_bodies=player_bodies.replace(needle,"mePlayerState = E_RW_MOVIE_PLAYER_CONSTRUCTED;",1)
if a.old_release:
 needle="if (mpRwVideoRenderer != nullptr)"
 assert player_bodies.count(needle)==2
 index=player_bodies.index(needle)
 player_bodies=player_bodies[:index]+"RetireArenaTexturesPC();\n        "+player_bodies[index:]
manager=tree.read("src/GameSource/Gui/BrnGuiMovieManager.cpp")
manager_bodies="\n".join(definition(manager,key) for key in (
 "bool MovieManager::PrepareMovieAllocator()", "void MovieManager::DestroyMemoryResourceAndDescriptor()",
 "void MovieManager::Update()", "void MovieManager::HandlePlayVideoEvent("))
manager_bodies=manager_bodies.replace("MovieManager::", "MovieManagerFixture::")
manager_bodies += "\n" + definition(manager, "void MovieManager::VideoDefinition::Copy(")
if a.old_keep:
 needle="if (mPlayingMovie.mbKeepMemoryWhenFinished)"
 assert needle in manager_bodies
 manager_bodies=manager_bodies.replace(needle,"if (false)")
header=tree.read("src/GameSource/Gui/BrnGuiMovieManager.h")
enums="\n".join(definition(header,"enum "+key)+";" for key in (
 "EMovieManagerState","ECollisionWorldState","ECarPoolState"))
texture=tree.read("src/pc/gcm/renderengine/texture.cpp")
texture_bodies=definition(texture,"rw::ResourceDescriptor* Texture2D::GetResourceDescriptor(")+"\n"+definition(texture,"Texture2D* Texture2D::Initialize(rw::Resource*")
tpl=tree.read(root+"ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp")
tpl_bodies="\n".join("template <typename V>\n"+definition(tpl,key) for key in (
 "void ImRenderBuffer<V>::Construct()", "bool ImRenderBuffer<V>::Prepare(",
 "void ImRenderBuffer<V>::Clear()", "void ImRenderBuffer<V>::Swap()",
 "V* ImRenderBuffer<V>::AllocVertices(", "void ImRenderBuffer<V>::BeginRendering()",
 "void ImRenderBuffer<V>::EndRendering()", "void ImRenderBuffer<V>::Render(",
 "void ImRenderBuffer<V>::SetProgram(",
 "void ImRenderBuffer<V>::SetState(const renderengine::RasterizerState*",
 "void ImRenderBuffer<V>::SetState(const renderengine::TextureState*",
 "void ImRenderBuffer<V>::SetState(const renderengine::BlendState*",
 "void ImRenderBuffer<V>::SetTransform(const Im2dTransform&",
 "const ImCommand* ImRenderBuffer<V>::GetFirstCommand() const",
 "const ImCommand* ImRenderBuffer<V>::GetNextCommand(",
 "void ImRenderBuffer<V>::SetBufferFullRewindToLastEndRender()"))
tpl_bodies += "\n" + "\n".join("template void ImRenderBuffer<Basic2dColouredTexturedVertex>::"+key+";" for key in (
 "BeginRendering()", "EndRendering()", "Render(renderengine::PrimitiveType, const Basic2dColouredTexturedVertex*, u32)",
 "SetProgram(s8)", "SetState(const renderengine::RasterizerState*)", "SetState(const renderengine::TextureState*)",
 "SetState(const renderengine::BlendState*)", "SetTransform(const Im2dTransform&)"))
code="namespace renderengine {\n"+texture_bodies+"\n}\nnamespace CgsGraphics {\n"+tpl_bodies+"\n"+player_bodies+"\n}\nnamespace BrnGui {\n"+manager_bodies+"\n}\n"
shadow = {}
if a.old_rectangle:
 event_header = "src/GameSource/Gui/BrnGuiVideoEvents.h"
 event_source = tree.read(event_header)
 needle = "mafRectangle[2] = 1.0f;    mafRectangle[3] = 1.0f;"
 assert event_source.count(needle) == 1
 shadow[event_header] = event_source.replace(needle,
     "mafRectangle[2] = 1280.0f; mafRectangle[3] = 720.0f;", 1)
result=compile_and_run(Path(__file__).with_name("MovieTextureOwnership.cpp"),
 "movie_texture_ownership.inc",code,"MovieTextureOwnership",shadow=shadow,extra_files={"movie_manager_enums.inc":enums},extra_sources=[
 REPO/root/"MoviePlayer/CgsMovieVideoRenderer.cpp",
 REPO/root/"MoviePlayer/CgsMoviePlayerCtor.cpp",
 REPO/"src/GameSource/Gui/BrnGuiMovieAllocator.cpp",
 REPO/"src/SDKs/EATech/rwmovie/videorenderable.cpp",
 REPO/"src/pc/gcm/renderengine/texturestate.cpp",
 REPO/"src/GameShared/GameClasses/Memory/CgsHeapMalloc.cpp",
 REPO/"src/GameShared/GameClasses/Memory/CgsLinearMalloc.cpp",
 REPO/"vendor/PPMalloc/src/EAGeneralAllocator.cpp",
 REPO/"vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
 REPO/"vendor/renderware/src/rwcore_alloc.cpp",
 REPO/"src/SDKs/EATech/eajobs/event.cpp",
 REPO/"src/SDKs/EATech/eajobs/bucket_list_node.cpp",
 REPO/"src/SDKs/EATech/eajobs/jobs.cpp",
 REPO/"vendor/coreallocator/source/icoreallocator_interface.cpp"])
if result is None: raise SystemExit(1)
checks,failures=result
print(f"run_movie_texture_ownership: {checks-failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
