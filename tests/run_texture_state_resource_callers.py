"""Exercise the real caller bodies against the canonical resource-backed initializer."""
from pathlib import Path
import os
import sys
from fxgs_common import Tree, definition, compile_and_run, REPO
os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
tree = Tree()
apt = tree.read("src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.cpp")
im = tree.read("src/GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.cpp")
apt_bodies = "\n".join(definition(apt, key) for key in (
    "    renderengine::TextureState* AptRenderHandler::TextureStateCache::InsertSorted(",
    "    static void BuildTextureStateParameters(",
    "    static renderengine::TextureState* ResolveTextureState("))
im_body = definition(im, "const TextureState* ImRendererBase::ConstructDefaultTextureState(")
if "--raw-lane-input" in sys.argv:
    apt_bodies = apt_bodies.replace("Initialize(&lResource, &lParams)",
        "Initialize(reinterpret_cast<rw::Resource*>(lResource.m_baseResources[0]), &lParams)", 1)
    im_body = im_body.replace("Initialize(&lStateResource, &lParameters)",
        "Initialize(reinterpret_cast<rw::Resource*>(lStateResource.m_baseResources[0]), &lParameters)", 1)
code = "namespace CgsGui {\n" + apt_bodies + "\n}\nnamespace CgsGraphics {\n" + im_body + "\n}\n"
result = compile_and_run(Path(__file__).with_name("TextureStateResourceCallers.cpp"),
    "texture_state_resource_callers.inc", code, "TextureStateResourceCallers", extra_sources=[
        REPO / "src/pc/gcm/renderengine/texturestate.cpp",
        REPO / "src/GameShared/GameClasses/Containers/CgsLinkedList.cpp",
        REPO / "vendor/renderware/src/rwcore_alloc.cpp",
        REPO / "vendor/renderware/src/rw/BaseResourceDescriptor.cpp",
        REPO / "vendor/PPMalloc/src/EAGeneralAllocator.cpp",
        REPO / "vendor/coreallocator/source/icoreallocator_interface.cpp"])
if result is None:
    raise SystemExit(1)
checks, failures = result
print(f"run_texture_state_resource_callers: {checks-failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
