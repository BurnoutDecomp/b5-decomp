from pathlib import Path
import collections
import json
import re
import subprocess

import strip_host as s
import finish_cleanup as f

stats = collections.Counter()
KEEP_NATIVE = {"Camera::UpdatePerspectiveProjectionMatrix", "Device::ResetProgramShadows",
               "AptGetClipMovie", "GetClipMovieEmbedded", "AptCallbackFile::LoadAnimation"}
changed = subprocess.check_output(["git", "diff", "--name-only", "--diff-filter=M"], cwd=s.ROOT, text=True).splitlines()
proc = subprocess.Popen(["git", "cat-file", "--batch"], cwd=s.ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
for name in changed:
    if Path(name).suffix not in s.SOURCE_SUFFIXES or s.excluded(name): continue
    data = (s.ROOT / name).read_bytes()
    proc.stdin.write(("HEAD:" + name + "\n").encode()); proc.stdin.flush()
    header = proc.stdout.readline().decode().split()
    old = proc.stdout.read(int(header[-1])); proc.stdout.read(1)
    if b"PC-platform leaf" not in old: continue
    original_host_methods = set()
    for node in s.walk(s.PARSER.parse(old).root_node):
        if node.type != "comment" or s.containing(node, "function_definition"): continue
        marker = s.text_of(node, old)
        if not re.search(r"FLAG PC-platform leaf|FLAG PC platform leaf", marker, re.I): continue
        if re.search(r"CORRECTED|was FLAG|banner|see |pointer widening", marker, re.I): continue
        target = s.marker_target(node, old)
        if target and target.type == "function_definition":
            for method in s.declarator_names(target, old):
                if method not in KEEP_NATIVE:
                    original_host_methods.add(method)
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type == "function_definition" and any(n in original_host_methods for n in s.declarator_names(node, data)):
            spans.append(s.erase_span(node, data, True)); stats["host leaf implementations"] += 1
    if spans: (s.ROOT / name).write_bytes(s.apply_spans(data, spans))
proc.stdin.close(); proc.wait()

host_apis = set()
inventory = json.loads((s.OUT / "host_api_inventory.json").read_text())
for api in inventory["apis"]:
    if re.search(r"FrameProfile::|WorldShader|Sampler_ApplyState|Probe|FrameDump|WorldDrawCallCount|OnEntryFreed|OnResourceMemoryFreed|SetWorldPassDefaultStates|ImShaderConstants_Flush|Im2dCompositeBlit|shadow::DeviceSet", api):
        host_apis.add(api)

for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type != "qualified_identifier": continue
        value = re.sub(r"\s+", "", s.text_of(node, data))
        if value not in host_apis: continue
        current = node
        while current and current.type not in s.STATEMENTS | s.DECLARATIONS:
            current = current.parent
        if not current: continue
        if current.type == "return_statement": current = s.containing(current, "function_definition")
        if current:
            if current.parent and current.parent.type == "condition_clause": current = current.parent.parent
            if current.parent and current.parent.type == "if_statement": current = current.parent
            spans.append(s.erase_span(current, data, True)); stats["host renderer hooks"] += 1
    if spans: path.write_bytes(s.apply_spans(data, spans))

# Remove obsolete host-port annotations after removing their implementations.
# Keep the code and native evidence outside those comment paragraphs intact.
for name in s.source_files():
    path = s.ROOT / name
    data = path.read_bytes()
    spans = []
    for node in s.walk(s.PARSER.parse(data).root_node):
        if node.type != "comment": continue
        text = s.text_of(node, data)
        if not re.search(r"FLAG PC|PC.platform leaf|PCLeaf|PC DECODE SUBSTITUTION", text, re.I): continue
        first = last = node
        while first.prev_named_sibling and first.prev_named_sibling.type == "comment":
            prev = first.prev_named_sibling
            if prev.end_point.row + 1 < first.start_point.row or re.fullmatch(r"//\s*", s.text_of(prev, data)):
                break
            line = data.rfind(b"\n", 0, prev.start_byte) + 1
            if data[line:prev.start_byte].strip(): break
            first = prev
        while last.next_named_sibling and last.next_named_sibling.type == "comment":
            nxt = last.next_named_sibling
            if last.end_point.row + 1 < nxt.start_point.row or re.fullmatch(r"//\s*", s.text_of(nxt, data)):
                break
            last = nxt
        start, _ = s.erase_span(first, data)
        _, end = s.erase_span(last, data)
        spans.append((start, end)); stats["obsolete host annotations"] += 1
    if spans: data = s.apply_spans(data, spans)
    text = data.decode("utf-8-sig").replace("\r\n", "\n")
    text = re.sub(r"[ \t]+\n", "\n", text)
    # Erased members can leave an empty namespace or anonymous implementation scope.
    text = re.sub(r"\bnamespace(?:\s+[\w:]+)?\s*\{\s*\}", "", text)
    text = re.sub(r"\n(?:[ \t]*\n){2,}", "\n\n", text)
    result = text.replace("\n", "\r\n").encode("utf-8")
    # Avoid line-ending-only churn in untouched reconstructed files.
    if spans or data.replace(b"\r\n", b"\n") != result.replace(b"\r\n", b"\n"):
        path.write_bytes(result)

f.cmake_cleanup()
print(dict(stats))
