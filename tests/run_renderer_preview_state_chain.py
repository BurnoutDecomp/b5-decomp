"""Execute the preview/Basic2d production bodies against recording device boundaries.

The workflow root is discovered by walking ancestors, or selected explicitly with
--workflow-root. Canonical include order is retained, with revision header shadows
first and private source paths substituted in their existing canonical slots.
The fixture records bind requests and CPU vertices; it does not create a GPU device.
"""
import argparse
import os
from pathlib import Path
import re
import sys

import fxgs_common as fx


def workflow_root(explicit):
    candidates = [explicit] if explicit else fx.REPO.parents
    for candidate in candidates:
        if candidate and (candidate / "tools/build/msvc_flags.txt").is_file():
            return candidate.resolve()
    raise SystemExit("workflow root not found; provide --workflow-root")


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--workflow-root", type=Path)
parser.add_argument("--rev", help="read production bodies and changed headers from this b5 revision")
parser.add_argument("--negative", choices=("reverse-rgba", "drop-active", "drop-reset",
    "stale-slot", "retain-owner", "bypass-raster-cache"))
parser.add_argument("--emit-only", type=Path, help="save extracted bodies without compiling or running")
args = parser.parse_args()
fx.WORKFLOW = workflow_root(args.workflow_root)
canonical_settings = fx.settings


def private_settings(name):
    values = canonical_settings(name)
    if name != "msvc_includes.txt":
        return values
    mapped = []
    for value in values:
        relative = Path(value)
        parts = relative.parts
        if parts[:2] == ("b5-decomp", "src"):
            mapped.append(str(fx.REPO.joinpath(*parts[1:])))
        else:
            mapped.append(str(fx.WORKFLOW / relative))
    return mapped


fx.settings = private_settings
tree = fx.Tree(args.rev)
im_path = "src/GameShared/GameClasses/Graphics/ImmediateMode/"
dispatch_path = "src/GameShared/GameClasses/Graphics/Dispatch/"
im = tree.read(im_path + "CgsIm2d.cpp")
base = tree.read(im_path + "CgsImRenderer.cpp")
shadow = tree.read(dispatch_path + "shadowingdevice.cpp")
module = tree.read("src/GameSource/Graphics/BrnRendererModule.cpp")
module_header = tree.read("src/GameSource/Graphics/BrnRendererModule.h")
texture = tree.read("src/pc/gcm/renderengine/texturestate.cpp")

try:
    # Keep the complete native helper declarations used by the real Render body.
    helper_start = im.index("namespace\n{")
    helpers = im[helper_start:im.index("namespace CgsGraphics", helper_start)]
    templates = "\n".join("template <typename V>\n" + fx.definition(im, signature)
        for signature in ("void ImRenderer<V>::BeginRendering()",
            "void ImRenderer<V>::EndRendering()", "void ImRenderer<V>::Render(",
            "void Im2dBase<V>::BeginRendering()"))
    screen = fx.definition(im, "static const Im2dTransform& ScreenSpaceIm2dTransform()")
    base_bodies = "\n".join(fx.definition(base, signature) for signature in (
        "void* ImRendererBase::SetStateLowLevel(",
        "void ImRendererBase::SetState(const BlendState*",
        "void ImRendererBase::SetState(const DepthStencilState*",
        "void ImRendererBase::SetState(const RasterizerState*",
        "void ImRendererBase::SetTexture(renderengine::Texture*",
        "void ImRendererBase::SetState(const renderengine::TextureState*"))
    active = re.search(r"^ImRendererBase\*\s+ImRendererBase::mgpActiveRenderer[^;]*;",
        base, re.M).group(0)
    static_start = shadow.index("bool Device::mbVertexProgramStateDirty")
    statics = shadow[static_start:shadow.index("bool Device::Initialize()", static_start)]
    cache = "\n".join(fx.definition(shadow, signature) for signature in (
        "void Device::ResetShadowing()", "void* Device::SetState(void*",
        "void* Device::SetResource(",
        "void* Device::SetState(const renderengine::TextureState*",
        "void* Device::GetSamplerTextureShadow(",
        "void Device::SetState(const renderengine::DepthStencilState*",
        "void Device::SetState(const renderengine::RasterizerState*",
        "void Device::SetState(const renderengine::BlendMaterialState*"))
    initialize = fx.definition(texture, "TextureState* TextureState::Initialize(")
    preview = fx.definition(module, "void BrnRendererModule::DebugDisplayRenderTarget(")
    frame = fx.code_only(fx.definition(module, "void BrnRendererModule::Render("))
except (ValueError, AttributeError) as error:
    raise SystemExit("required production body or static missing: " + str(error))

if args.negative == "reverse-rgba":
    before = ("static_cast<u8>(luColour & 0xFFu),\n"
        "        static_cast<u8>((luColour >> 8) & 0xFFu),\n"
        "        static_cast<u8>((luColour >> 16) & 0xFFu),\n"
        "        static_cast<u8>((luColour >> 24) & 0xFFu)")
    after = ("static_cast<u8>((luColour >> 24) & 0xFFu),\n"
        "        static_cast<u8>((luColour >> 16) & 0xFFu),\n"
        "        static_cast<u8>((luColour >> 8) & 0xFFu),\n"
        "        static_cast<u8>(luColour & 0xFFu)")
    assert before in preview, "negative control did not match packed color"
    preview = preview.replace(before, after, 1)
elif args.negative == "drop-active":
    before = "mgpActiveRenderer = static_cast<ImRendererBase*>(this);"
    assert before in templates
    templates = templates.replace(before, "", 1)
elif args.negative == "drop-reset":
    assert "shadow::Device::ResetShadowing();" in templates
    templates = templates.replace("shadow::Device::ResetShadowing();", "", 1)
elif args.negative == "stale-slot":
    assert "mi8CurrentProgram = 0;" in templates
    templates = templates.replace("mi8CurrentProgram = 0;", "", 1)
elif args.negative == "retain-owner":
    assert "mgpActiveRenderer = nullptr;" in templates
    templates = templates.replace("mgpActiveRenderer = nullptr;", "", 1)
elif args.negative == "bypass-raster-cache":
    before = "shadow::Device::SetState(reinterpret_cast<const renderengine::RasterizerState*>(lpState));"
    assert before in base_bodies
    base_bodies = base_bodies.replace(before,
        "shadow::Device::Xbox2SetRasterizerStateLowLevelShadowed("
        "const_cast<RasterizerState*>(lpState), false);", 1)

code = (helpers + "\nnamespace shadow {\n" + statics + cache + "\n}\n"
    "namespace renderengine {\n" + initialize + "\n}\n"
    "namespace CgsGraphics {\n" + active + "\n" + base_bodies + "\n" + screen
    + "\n" + templates + "\n}\n" + preview + "\n")

publication = frame.find("suOcclusionCullIndexCountThreshold = muOcclusionCullIndexCountThreshold;")
readers = ("mLoadingScreenRenderer.AddCommand(", "BuildDispatchLists(",
    "RenderShadowMapPasses(", "BeginRenderEnvironmentMapFace(")
wiring = [("threshold is published before " + reader,
    0 <= publication < frame.find(reader)) for reader in readers]
header_code = fx.code_only(module_header)
wiring += [("preview owner has canonical sampler parameters and resource members",
    bool(re.search(r"renderengine::TextureState::Parameters\s+mTextureStateParams;", header_code))
    and bool(re.search(r"rw::Resource\s+mTextureStateResource;", header_code))),
    ("preview owner stores the native TextureState pointer",
    bool(re.search(r"renderengine::TextureState\*\s+mpTextureState;", header_code)))]

if args.emit_only:
    args.emit_only.mkdir(parents=True, exist_ok=True)
    (args.emit_only / "renderer_preview_state_chain.inc").write_text(code, encoding="utf-8")
    for label, passed in wiring:
        print(("PASS " if passed else "FAIL ") + label)
    raise SystemExit(any(not passed for _, passed in wiring))

os.environ.pop("NoDefaultCurrentDirectoryInExePath", None)
headers = (im_path + "CgsImRenderer.h", im_path + "CgsIm2d.h",
    dispatch_path + "shadowingdevice.h")
revision_headers = {name: tree.read(name) for name in headers} if args.rev else None
numeric = fx.compile_and_run(Path(__file__).with_name("RendererPreviewStateChain.cpp"),
    "renderer_preview_state_chain.inc", code, "RendererPreviewStateChain", shadow=revision_headers)
for label, passed in wiring:
    print(("PASS " if passed else "FAIL ") + label)
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
failures += sum(not passed for _, passed in wiring)
print(f"run_renderer_preview_state_chain: {checks + len(wiring) - failures}/"
    f"{checks + len(wiring)} pass ({failures} fail)")
raise SystemExit(bool(failures))
