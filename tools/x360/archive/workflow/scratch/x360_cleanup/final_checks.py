from pathlib import Path
import re
import subprocess
import strip_host as s

stub_methods = {"AiffWriter::RwacTimerClient", "AiffWriter_DestructTimerHandle",
                "DelayLine::SetDelay", "DelayFilterCrossFadeFunc", "Pan2D1::ComputeInteriorTerm"}
for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type == "function_definition" and any(n in stub_methods for n in s.declarator_names(node, data)):
            spans.append(s.erase_span(node, data, True))
        if node.type == "preproc_function_def" and b"FLAG PC" in data[node.start_byte:node.end_byte]:
            spans.append(s.erase_span(node, data))
        if node.type == "comment" and re.search(rb"PC[- ]leaf|PCLeaf", data[node.start_byte:node.end_byte], re.I):
            spans.append(s.erase_span(node, data))
    if spans: path.write_bytes(s.apply_spans(data, spans))

deleted = set(subprocess.check_output(["git", "diff", "--name-only", "--diff-filter=D"], cwd=s.ROOT, text=True).splitlines())
cmakes = [n for n in s.FILES if Path(n).name == "CMakeLists.txt" and not s.excluded(n) and (s.ROOT / n).exists()]
for name in cmakes:
    # Rebuild lists from their original text, pruning only this task's deleted files.
    text = subprocess.check_output(["git", "show", "HEAD:" + name], cwd=s.ROOT).decode("utf-8-sig").replace("\r\n", "\n")
    kept = []
    for line in text.splitlines(keepends=True):
        if re.match(r"\s*add_subdirectory\(\s*[Pp][Cc]\s*\)", line): continue
        match = re.match(r'\s*"?([^"\n]+\.(?:cpp|h|hpp|c|inl))"?\s*$', line)
        if match:
            value = match.group(1).strip().replace("${CMAKE_CURRENT_SOURCE_DIR}/", "")
            target = (Path(name).parent / value).as_posix()
            if target in deleted: continue
        kept.append(line)
    text = "".join(kept)
    text = re.sub(r"if \(WIN32\)\n\s*endif\(\)\n", "", text)
    if name == "src/CMakeLists.txt":
        text = re.sub(r"if \(WIN32\)\n    add_executable.*?endif\(\)\n", 'add_executable(${PROJECT_NAME} "")\n\ntarget_compile_definitions(${PROJECT_NAME} PRIVATE\n    D_PLATFORM_X360=1\n    CGS_PLATFORM_X360=1\n    _XBOX=1\n)\n', text, count=1, flags=re.S)
        text = re.sub(r"if \(WIN32\)\n.*?endif\(\)\n", "", text, flags=re.S)
    if name == "src/GameShared/GameClasses/System/CMakeLists.txt":
        text = text.replace("    PRIVATE\n", "    PRIVATE\n        PS3/CgsHardwareInitPS3.cpp\n        PS3/CgsHardwareLanguagePS3.cpp\n        PS3/CgsHardwareSkuPS3.cpp\n        X360/CgsXOverlappedX360.cpp\n", 1)
    (s.ROOT / name).write_bytes((text.rstrip() + "\n").replace("\n", "\r\n").encode("utf-8"))

# Clean only files already changed by this task.
changed = subprocess.check_output(["git", "diff", "--name-only", "--diff-filter=M"], cwd=s.ROOT, text=True).splitlines()
for name in changed:
    path = s.ROOT / name
    if path.suffix not in s.SOURCE_SUFFIXES | {".txt", ".md"}: continue
    if s.excluded(name): continue
    data = path.read_bytes().replace(b"\r", b"")
    data = re.sub(rb"[ \t]+\n", b"\n", data)
    data = re.sub(rb"\n(?:[ \t]*\n){2,}", b"\n\n", data)
    data = data.rstrip() + b"\n"
    path.write_bytes(data.replace(b"\n", b"\r\n"))

print("Removed five link-closure stand-ins and two unused desktop diagnostic macros; bounded build-list pruning")
