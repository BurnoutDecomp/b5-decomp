from pathlib import Path
import sys


here = Path(__file__).resolve().parent
source = (Path(sys.argv[1]) /
          "src/GameShared/GameClasses/Graphics/Font/CgsFontRenderer.cpp")
body = source.read_text(encoding="utf-8")
start = body.index("    void TextRenderer::RenderStringInternal(")
end = body.index("\n    void TextRenderer::", start + 1)
before, render, after = body[:start], body[start:end], body[end:]
anchor = "        const CgsResource::Font* lpFont = lrTextObject.mpFont.operator->();"
assert render.count(anchor) == 1, anchor
render = render.replace(
    anchor,
    anchor + """
        static unsigned int suDrawProbe = 0;
        if (suDrawProbe++ < 32)
        {
            char lacProbe[180];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R108_TEXT_RENDER font=%08X textureState=%08X family=%.64s\\n",
                reinterpret_cast<u32>(lpFont),
                lpFont ? reinterpret_cast<u32>(lpFont->mpTextureState) : 0u,
                lpFont ? lpFont->macTypefaceFamilyName : "<NULL>");
            OutputDebugStringA(lacProbe);
        }""",
)
with (here / source.name).open("x", encoding="utf-8") as output:
    output.write('#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'
                 + before + render + after)
