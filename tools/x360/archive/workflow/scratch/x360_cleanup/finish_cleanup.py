from pathlib import Path
import collections
import json
import re
import subprocess

import strip_host as s

EXTRA_NAMES = {
    "FrameInterpolation", "PoseTrack", "gbDecoupleSimulationFromRenderRate",
    "mbTimingReset", "mfInterpolationAlpha", "mbDecoupledFromRenderRate",
    "mi64PerformanceFrequency", "mfLastFrameSeconds", "GetInterpolationAlpha",
    "IsDecoupledFromRenderRate", "GetLastFrameSeconds", "PcmTrace", "ShadowCamProbe",
    "gShadowCamProbe", "IOBufferHostZeroFillEnabled", "IsPortedLayout", "IsStaleBlob",
    "AptPromptDiagnosticsPC", "ProgramBufferPC", "GetExternalBootFrame",
}
DESKTOP_API = re.compile(rb"\b(?:PeekMessage[A-W]?|TranslateMessage|DispatchMessage[A-W]?|"
                         rb"PostThreadMessage[A-W]?|PostQuitMessage|GetAsyncKeyState|"
                         rb"MsgWaitForMultipleObjects(?:Ex)?|SetThreadDescription|"
                         rb"SetWindowLongPtr[A-W]?|CreateWindow(?:Ex)?[A-W]?|"
                         rb"DefWindowProc[A-W]?|DestroyWindow|ShowWindow|SetWindowPos)\s*\(")


def tidy_sources():
    stats = collections.Counter()
    for name in s.source_files():
        path = s.ROOT / name
        data = path.read_bytes()
        root = s.PARSER.parse(data).root_node
        spans = []
        replacements = []
        local_names = collections.defaultdict(set)
        global_names = set(EXTRA_NAMES)

        def remove(node, reason):
            while node.parent:
                parent = node.parent
                if parent.type in {"template_declaration", "condition_clause", "init_statement"}:
                    node = parent
                elif parent.type == "else_clause":
                    node = parent
                elif parent.type == "if_statement" and parent.child_by_field_name("consequence") == node:
                    alternate = parent.child_by_field_name("alternative")
                    if alternate:
                        body = alternate.named_children[-1]
                        replacements.append((parent.start_byte, parent.end_byte, data[body.start_byte:body.end_byte]))
                        stats[reason] += 1
                        return
                    node = parent
                elif parent.type in {"if_statement", "while_statement", "for_statement", "switch_statement"} and parent.child_by_field_name("condition") == node:
                    node = parent
                else:
                    break
            spans.append(s.erase_span(node, data))
            stats[reason] += 1

        nodes = list(s.walk(root))
        for node in nodes:
            code = data[node.start_byte:node.end_byte]
            if node.type == "function_definition":
                names = s.declarator_names(node, data)
                if DESKTOP_API.search(code) or b"std::chrono::" in code or b"std::this_thread::" in code:
                    remove(node, "desktop implementation")
                if any(re.search(r"Diag|Witness|Harness|Trace|Probe", n) for n in names) and re.search(rb'"BRN_[A-Z0-9_]+"', code):
                    remove(node, "host diagnostic function")
                    for qualified in names:
                        global_names.add(qualified.split("::")[-1])
            if node.type in {"static_assert_declaration", "expression_statement"} and b"static_assert" in code:
                if re.search(rb"x64|64.bit|XB1|Xbox.?One|host|sizeof\s*\(\s*void\s*\*\s*\)\s*==\s*8", code, re.I):
                    remove(node, "host ABI assertion")
            if node.type == "if_statement":
                cond = node.child_by_field_name("condition")
                if cond and not re.sub(rb"[()\s]", b"", data[cond.start_byte:cond.end_byte]):
                    remove(node, "removed host condition")
            if node.type in {"string_literal", "raw_string_literal"}:
                if b"FLAG PC" in code:
                    statement = node
                    while statement and statement.type not in {"expression_statement", "declaration"}:
                        statement = statement.parent
                    if statement: remove(statement, "host diagnostic output")
                if re.match(rb'"BRN_[A-Z0-9_]+"$', code):
                    current = node
                    while current and current.type not in {"declaration", "if_statement", "return_statement", "expression_statement"}:
                        current = current.parent
                    if not current: continue
                    if current.type == "declaration":
                        names = s.declarator_names(current, data)
                        function = s.containing(current, "function_definition")
                        dest = local_names[function.start_byte] if function else global_names
                        dest.update(n.split("::")[-1] for n in names)
                        remove(current, "host environment override")
                    elif current.type == "return_statement":
                        fn = s.containing(current, "function_definition")
                        if fn:
                            global_names.update(n.split("::")[-1] for n in s.declarator_names(fn, data))
                            remove(fn, "host environment predicate")
                    else:
                        remove(current, "host environment hook")

        for node in nodes:
            if node.type not in s.IDENTIFIERS: continue
            token = s.text_of(node, data)
            fn = s.containing(node, "function_definition")
            names = global_names | (local_names[fn.start_byte] if fn else set())
            if token not in names: continue
            current = node
            while current and current.type not in s.STATEMENTS | s.DECLARATIONS:
                current = current.parent
            if not current: continue
            if current.type == "return_statement":
                if fn: remove(fn, "host-dependent implementation")
            else:
                remove(current, "host dependency")

        edits = [(a, b, b"") for a, b in spans] + replacements
        accepted = []
        for a, b, replacement in sorted(edits, key=lambda e: (e[0], -e[1])):
            if accepted and a < accepted[-1][1]: continue
            accepted.append((a, b, replacement))
        output = data
        for a, b, replacement in reversed(accepted): output = output[:a] + replacement + output[b:]
        if output != data: path.write_bytes(output)
    print(dict(stats))


def cmake_cleanup():
    changed = 0
    for name in s.FILES:
        path = s.ROOT / name
        if path.name != "CMakeLists.txt" or not path.exists() or "/pc/" in name.lower(): continue
        old = path.read_bytes()
        text = old.decode("utf-8-sig")
        result = []
        for line in text.splitlines(keepends=True):
            if re.match(r"\s*add_subdirectory\(\s*[\"']?[Pp][Cc][\"']?\s*\)", line): continue
            m = re.match(r'\s*[\"]?([^\"\r\n]+\.(?:cpp|h|hpp|c|inl))[\"]?\s*$', line)
            if m:
                target = path.parent / m.group(1).strip()
                if not target.exists(): continue
            result.append(line)
        output = "".join(result).encode("utf-8")
        if output != old:
            path.write_bytes(output); changed += 1
    print("Pruned CMake lists:", changed)


if __name__ == "__main__":
    tidy_sources()
    cmake_cleanup()
