from pathlib import Path
import re
import strip_host as s


def save(name, text):
    (s.ROOT / name).write_bytes(text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))


def read(name):
    return (s.ROOT / name).read_text(encoding="utf-8-sig")


for name in ["src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.cpp",
             "src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayerCtor.cpp"]:
    (s.ROOT / name).unlink(missing_ok=True)

name = "src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMoviePlayer.h"
text = read(name)
text = re.sub(r"// FFmpeg backend.*?struct SwsContext;\n", "", text, flags=re.S)
text = re.sub(r"        u64   muStartTick;.*?(?=        // ---- console decode pipeline)", "", text, flags=re.S)
text = re.sub(r"^\s*f32\s+ComputeCrossfadeAlpha\([^;]+;[^\n]*\n", "", text, flags=re.M)
save(name, text)

name = "src/GameShared/GameClasses/Graphics/MoviePlayer/CgsMovieVideoRenderer.cpp"
data = (s.ROOT / name).read_bytes()
spans = []
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type != "function_definition": continue
    code = s.text_of(node, data)
    if "MovieVideoRenderer::CreateOutputTexture(" in code or "MovieVideoRenderer::Render(s32" in code:
        spans.append(s.erase_span(node, data, True))
data = s.apply_spans(data, spans)
text = data.decode("utf-8").replace("\r\n", "\n")
text = text.replace('extern "C" {\n#include <libswscale/swscale.h>\n}\n', "")
text = text.replace("(sizeof(MovieVideoRenderer) + 127u) & ~127u", "0x500u")
save(name, text)

# The existing ARTIST path already identifies the four relocated 32-bit pointers.
# Remove the resource-relative native-8 indirection and its span diagnostics.
name = "src/GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.cpp"
text = read(name)
if "        const uintptr_t luHeaderBase" in text:
    start = text.index("        const uintptr_t luHeaderBase")
    end = text.index("        // Pin the caller's handle", start)
    text = text[:start] + "        void* lpBase = reinterpret_cast<void*>(lpAptData->mpAptData);\n        void* lpConstFile = reinterpret_cast<void*>(lpAptData->mpConstData);\n\n" + text[end:]
text = re.sub(r"^\s*gAptResourceSpan(?:Base|Size)\s*=.*?;\n", "", text, flags=re.M)
text = re.sub(r"^\s*(?:uintptr_t|uint32_t)\s+gAptResourceSpan(?:Base|Size)\s*=.*?;\n", "", text, flags=re.M)
save(name, text)

name = "src/GameSource/Game/BrnGameModule.cpp"
text = read(name)
text = text.replace(", mi8FrameRateMinSteps(0)", ", mi8FrameRateMinSteps(1)")
text = text.replace("static u8 saUpdateOutputMem[0x1000000];", "static u8 saUpdateInputMem[0x780000];\n        static u8 saUpdateOutputMem[0x780000];")
text = re.sub(r"^\s*lParams\.miVariable(?:Metadata)?PoolSize\s*\+=\s*\d+;\n", "", text, flags=re.M)
text = text.replace("        if (!lbHighDef)\n", "        const bool lbHighDef = mRenderModule.IsHD();\n        if (!lbHighDef)\n", 1)
# A forced-HD branch was solely for the converted PC data set.
data = text.encode("utf-8")
spans = []
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type == "if_statement" and "lbHighDef = true" in s.text_of(node, data):
        spans.append(s.erase_span(node, data, True))
text = s.apply_spans(data, spans).decode("utf-8")
save(name, text)

for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type == "function_definition":
            if any(n.type in s.IDENTIFIERS and s.text_of(n,data) == "resolve64" for n in s.walk(node)):
                spans.append(s.erase_span(node, data, True))
        elif node.type in {"declaration", "field_declaration"} and any(n.type in s.IDENTIFIERS and s.text_of(n,data) == "resolve64" for n in s.walk(node)):
            spans.append(s.erase_span(node, data, True))
    if spans: path.write_bytes(s.apply_spans(data, spans))

print("Removed FFmpeg playback, native-8 Apt walkers and remaining converted-data paths")
