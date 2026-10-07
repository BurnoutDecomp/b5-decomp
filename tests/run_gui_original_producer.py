"""CPU checks for actual GUI producer Render/helper/snapshot and typed IO bodies.

--drop-forwarding removes only the original set publication; --private-subset
reproduces the old private-Im2d/null-Im3d replacement. --old-movie REV runs that
revision's unchanged movie Render wrapper against idle/playing/stopped checks.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, STRSTREAM_CPP

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--drop-forwarding", action="store_true")
parser.add_argument("--private-subset", action="store_true")
parser.add_argument("--old-movie", metavar="REV")
args = parser.parse_args()
tree = Tree()
module = tree.read("src/GameSource/Gui/BrnGuiModule.cpp")
render = definition(module, "void GuiModule::Render(")
publication = "lpViewInput->SetImRenderers(lpInput->GetImRenderers());"
if args.drop_forwarding:
    render = render.replace(publication, "", 1)
if args.private_subset:
    render = render.replace(publication, """CgsGui::ImRendererSet lPrivate = lpInput->GetImRenderers();
            lPrivate.mpIm2dRenderBuffer = reinterpret_cast<CgsGraphics::Im2dRenderBuffer*>(UINT64_C(0x12340000BAD00000));
            lPrivate.mpIm3dRenderBuffer = nullptr;
            lPrivate.mpIm3dRenderBufferUntex = nullptr;
            lpViewInput->SetImRenderers(lPrivate);""", 1)
module_bodies = "\n".join((render, definition(module, "void GuiModule::UpdateAndRenderMovieManager("),
    definition(module, "void GuiModule::CaptureRenderInputPC("),
    definition(module, "CgsGui::CgsGuiModuleIO::InputBuffer* GuiModule::GetCompletedRenderInputPC()")))
movie = definition(Tree(args.old_movie).read("src/GameSource/Gui/BrnGuiMovieManager.cpp"),
                   "void MovieManager::Render(")
input_source = tree.read("src/GameShared/GameClasses/Gui/CgsGuiModuleIO_InputBuffer.cpp")
input_bodies = "\n".join(definition(input_source, signature) for signature in (
    "void InputBuffer::SetCamera(", "void InputBuffer::SetImRenderers(",
    "const ImRendererSet& InputBuffer::GetImRenderers() const"))
view_source = tree.read("src/GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.cpp")
view_bodies = "\n".join(definition(view_source, signature) for signature in (
    "void InputBuffer::SetImRenderers(", "const ImRendererSet& InputBuffer::GetImRenderers() const"))
camera_source = tree.read("src/GameShared/GameClasses/Graphics/CgsCamera.cpp")
camera_copy = "\n".join(definition(camera_source, signature) for signature in (
    "Camera::Camera(const Camera&", "Camera& Camera::operator=("))
# Input constructor dependencies are already exercised by run_gui_im_renderer_set.
# Here the fixture constructs real base lock state; no private render buffer exists.
base = tree.read("src/GameShared/GameClasses/Module/CgsIOBuffer.cpp")
bodies = (base + "\nnamespace CgsGraphics {\n" + camera_copy + "\n}\n"
    + "namespace CgsGui { namespace CgsGuiModuleIO {\n" + input_bodies
    + "\n} namespace ViewIO {\n" + view_bodies + "\n} }\n"
    + "namespace BrnGui {\n" + movie + "\n" + module_bodies + "\n}\n")
# The fixture uses the production IOBuffer base constructor for forwarding checks;
# full module/camera construction is covered by run_gui_im_renderer_set.
fixture = Path(__file__).with_name("GuiOriginalProducer.cpp")
result = compile_and_run(fixture, "gui_original_producer.inc", bodies,
    "GuiOriginalProducer", extra_sources=[STRSTREAM_CPP])
if result is None:
    raise SystemExit(1)
checks, failures = result
print(f"run_gui_original_producer: {checks - failures}/{checks} pass ({failures} fail)")
raise SystemExit(bool(failures))
