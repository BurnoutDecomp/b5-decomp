from pathlib import Path
import re
import strip_host as s


def read(name):
    return (s.ROOT / name).read_text(encoding="utf-8-sig")


def save(name, text):
    (s.ROOT / name).write_bytes(text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8"))


(s.ROOT / "src/GameShared/GameClasses/Development/Log/CgsLog.cpp").unlink(missing_ok=True)

name = "src/SDKs/EATech/include/Apt/AptLoader.cpp"
text = read(name)
start = text.index("    // The data root (the movie root CHARACTER HEADER)")
end = text.index("    AptCharacterAnimation* pCharAnim =", start)
text = text[:start] + """    if (pConstFile->mnDataRootOffset != 0)
        pConstFile->mnDataRootOffset += reinterpret_cast<uintptr_t>(pBase);
    void* pRoot = reinterpret_cast<void*>(pConstFile->mnDataRootOffset);
    const unsigned int luHdrSize = 16u;

""" + text[end:]
start = text.index("    AptLifeLog(\"CompleteLd ", text.index("void AptLoader::CompleteLoad"))
end = text.index("    filePtr.pData = nullptr;", start)
text = text[:start] + "    pConstFile->mnDataRootOffset -= reinterpret_cast<uintptr_t>(pBase);\n    AptSharedPtr<AptFile>::Dispose(filePtr.pData);\n" + text[end:]
save(name, text)

name = "src/SDKs/EATech/include/Apt/AptCIHNativeFunctionHelper.cpp"
text = read(name)
text = re.sub(r"int AptShapeHitTest\(AptValue\* pNode, float fX, float fY\)\n\{.*?\n\}", """int AptShapeHitTest(AptValue* pNode, float fX, float fY)
{
    return gAptFuncs.pfnPointHitTest(fX, fY,
        static_cast<AptAssetMoiveClip>(reinterpret_cast<uintptr_t>(pNode)));
}""", text, count=1, flags=re.S)
text = text.replace("    float fBytesTotal = 0.0f;", "    const float fBytesTotal = static_cast<float>(gAptFuncs.pfnGetBytesTotal(\n        lFilePath.GetBuffer(), static_cast<AptGetBytesEnum>(0)));", 1)
data = text.encode("utf-8")
spans = []
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type in s.IDENTIFIERS and s.text_of(node,data).startswith("diagnostic"):
        current = node
        while current and current.type not in s.STATEMENTS | s.DECLARATIONS:
            current = current.parent
        if current:
            if current.parent and current.parent.type == "condition_clause": current = current.parent.parent
            spans.append(s.erase_span(current,data))
save(name, s.apply_spans(data,spans).decode("utf-8"))

name = "src/GameSource/Graphics/BrnRendererModule.cpp"
text = read(name)
needle = "            continue;\n\n            labStatFaceRendered[luFace] = true;"
assert needle in text
text = text.replace(needle, "            if (!lpDispatchThreadInputBuffer->GetEnvMapFaceRender(luFace))\n                continue;\n\n            labStatFaceRendered[luFace] = true;", 1)
data = text.encode("utf-8")
profile_names = {"EnvMapPerf", "sEnvMapPerf", "siEnvMapPerfLog", "lPerfFreq", "lfUsPerTick",
                 "lLoopT0", "lLoopT1", "lT0", "lT1", "lT2", "lT3", "lT4",
                 "labStatFaceRendered", "lauStatFaceMeshes", "sxEnvMapEverRendered", "suEnvMapBeginCount"}
spans=[]
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type not in s.IDENTIFIERS or s.text_of(node,data) not in profile_names: continue
    current=node
    while current and current.type not in s.STATEMENTS | s.DECLARATIONS: current=current.parent
    if current:
        if current.parent and current.parent.type == "condition_clause": current=current.parent.parent
        if current.parent and current.parent.type == "if_statement": current=current.parent
        spans.append(s.erase_span(current,data))
save(name,s.apply_spans(data,spans).decode("utf-8"))

name = "src/GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h"
text = read(name)
text = re.sub(r"^\s*u32\s+muPad[01];[^\n]*\n", "", text, flags=re.M)
text = re.sub(r"^\s*u32\s+muPad;[^\n]*\n", "", text, flags=re.M)
save(name,text)

name = "src/SDKs/EATech/include/Apt/AptCharacterAnimation.h"
text = read(name)
text = re.sub(r"^\s*uint8_t\s+mPad(?:04|1C|44)\[[^;]+;[^\n]*\n", "", text, flags=re.M)
text = re.sub(r"int64_t(\s+mnParsedValueCount)", r"int32_t\1", text)
text = re.sub(r"^static_assert\([^;]+;\n", "", text, flags=re.M)
text += "\nstatic_assert(sizeof(AptImportEntry) == 16, \"ARTIST import entry\");\nstatic_assert(sizeof(AptInitEntry) == 8, \"ARTIST init entry\");\nstatic_assert(offsetof(AptCharacterAnimation, mnParsedValueCount) == 0x30, \"ARTIST resolve counter\");\n"
save(name,text)

name = "src/SDKs/EATech/include/Apt/AptConstFile.h"
text = read(name)
data=text.encode("utf-8")
spans=[]
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type=='function_definition' and 'GetPointerSizeBytes' in s.declarator_names(node,data):spans.append(s.erase_span(node,data,True))
save(name,s.apply_spans(data,spans).decode('utf-8'))

# The 64-bit MOIR structs are a PC data-provider replacement. Keep the recovered
# registry API and leave its native subscription data to reconstruct.
name = "src/SDKs/Csis/CsisSystem.h"
data=(s.ROOT/name).read_bytes();spans=[]
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type=='struct_specifier' and any(n in {'SystemClient24','SystemClient32','SystemContent','SystemContentNode'} for n in s.declarator_names(node,data)):
        spans.append(s.erase_span(node,data,True))
    if node.type=='static_assert_declaration' and b'CSIS' in data[node.start_byte:node.end_byte]:spans.append(s.erase_span(node,data))
text=s.apply_spans(data,spans).decode('utf-8').replace("struct InterfaceId;", "struct InterfaceId;\nstruct SystemContent;\nstruct SystemContentNode;")
save(name,text)
name = "src/SDKs/Csis/CsisSystem.cpp"
data=(s.ROOT/name).read_bytes();spans=[]
for node in s.walk(s.PARSER.parse(data).root_node):
    if node.type=='function_definition' and any(n in {'System::Subscribe','System::Unsubscribe','System::ResolveInterface'} for n in s.declarator_names(node,data)):
        spans.append(s.erase_span(node,data,True))
save(name,s.apply_spans(data,spans).decode('utf-8'))

print("Restored native ownership, callbacks and face guard; removed desktop logger/profiler and 64-bit data-provider records")
