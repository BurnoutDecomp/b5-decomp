from pathlib import Path
import re
import subprocess
import strip_host as s

REMOVE_METHODS = {
    "StackUnpick::ComputeAdjustedStack", "StackUnpick::Prepare",
    "RwMallocMemAligned", "RwFreeMemAligned", "TrafficJobEntry",
    "EnvironmentManager::Prepare", "AptDataHeaderType::GetImportCount",
    "AptDataHeaderType::GetImportPointer", "AptResolveShapeGeometry",
    "AptMovie::DoTemporaryFrameControls", "AptMovie::doFrameControls",
    "AptMovie::queueFrameActions",
}

for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    replacements = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type == "function_definition":
            code = data[node.start_byte:node.end_byte]
            names = s.declarator_names(node,data)
            if any(n in REMOVE_METHODS for n in names) or re.search(rb"\b(?:GetModuleFileName[A-W]?|GetModuleHandle[A-W]?|CaptureStackBackTrace)\s*\(",code):
                spans.append(s.erase_span(node,data,True))
        if node.type == "call_expression":
            function = node.child_by_field_name("function")
            if function and s.text_of(function,data).endswith("GetPointerSizeBytes"):
                replacements.append((node.start_byte,node.end_byte,b"4"))
    edits=[(a,b,b"") for a,b in spans]+replacements
    accepted=[]
    for a,b,repl in sorted(edits,key=lambda x:(x[0],-x[1])):
        if accepted and a<accepted[-1][1]:continue
        accepted.append((a,b,repl))
    for a,b,repl in reversed(accepted):data=data[:a]+repl+data[b:]
    if accepted:path.write_bytes(data)

# Native traffic submission and blocking wait are already attested in this TU.
name = "src/GameSource/Jobs/Traffic/BrnTrafficJob.cpp"
text=(s.ROOT/name).read_text(encoding="utf-8-sig")
text=text.replace('    CGS_ASSERT(mbRunningJob, "mbRunningJob");\n    mbRunningJob = false;', '    CGS_ASSERT(mbRunningJob, "mbRunningJob");\n    mJob.WaitOn(0, 0, -1);\n    mbRunningJob = false;',1)
text=re.sub(r"    \{\n        TrafficJobEntry\(EA::Jobs::Param\(\).*?\n    \}", "    CgsSystem::JobManager()->AddJobs(&mJob, 1);",text,count=1,flags=re.S)
(s.ROOT/name).write_bytes(text.replace("\n","\r\n").encode("utf-8"))

# The allocation engine is a PC substitute. Retain its native API headers only.
path=s.ROOT/"vendor/PPMalloc/src/EAGeneralAllocator.cpp"
path.unlink(missing_ok=True)
path=s.ROOT/"vendor/PPMalloc/CMakeLists.txt"
text=path.read_text()
text=re.sub(r"target_sources\(PPMalloc INTERFACE\s*\"\$\{CMAKE_CURRENT_SOURCE_DIR\}/src/EAGeneralAllocator.cpp\"\s*\)\s*", "",text)
path.write_bytes(text.replace("\n","\r\n").encode("utf-8"))

for name in s.FILES:
    path=s.ROOT/name
    if name.startswith('vendor/') and path.suffix in s.SOURCE_SUFFIXES and path.exists() and '/pc/' not in name.lower():
        data=path.read_bytes()
        output=re.sub(rb'(?i)(#\s*include\s*)[<\"]windows\.h[>\"]',rb'\1<xtl.h>',data)
        if output!=data:path.write_bytes(output)

# Fold the remaining file-width choices to the existing 32-bit source arm.
for name in s.source_files():
    path=s.ROOT/name;data=path.read_bytes()
    if b'GetPointerSizeBytes' not in data and not re.search(rb'\b(?:liPtrSize|nPtrSize|nPointerSize)\b',data):continue
    constants={}
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type=='declaration':
            m=re.fullmatch(r'(?:const\s+)?(?:int|u32|unsigned int)\s+(\w+)\s*=\s*4\s*;',s.text_of(node,data).strip())
            if m:constants[m.group(1)]='4'
    replacements=[]
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type not in {'if_statement','conditional_expression'}:continue
        cond=node.child_by_field_name('condition')
        if not cond:continue
        expression=s.text_of(cond,data)
        for variable,value in constants.items():expression=re.sub(r'\b'+re.escape(variable)+r'\b',value,expression)
        decision=s.evaluate(expression)
        if decision is None:continue
        chosen=node.child_by_field_name('consequence' if decision else 'alternative')
        if chosen and chosen.type=='else_clause':chosen=chosen.named_children[-1]
        replacements.append((node.start_byte,node.end_byte,data[chosen.start_byte:chosen.end_byte] if chosen else b''))
    accepted=[]
    for item in sorted(replacements,key=lambda x:(x[0],-x[1])):
        if accepted and item[0]<accepted[-1][1]:continue
        accepted.append(item)
    for a,b,repl in reversed(accepted):data=data[:a]+repl+data[b:]
    if accepted:path.write_bytes(data)

print("Removed desktop stack/map/allocation implementations and native-8 readers; restored native traffic submission/wait")
