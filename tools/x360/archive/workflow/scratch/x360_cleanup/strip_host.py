from pathlib import Path
import ast
import collections
import json
import re
import subprocess
import sys

from tree_sitter import Language, Parser
import tree_sitter_cpp

sys.stdout.reconfigure(encoding="utf-8")
ROOT = Path(__file__).resolve().parents[2] / "b5-decomp"
OUT = Path(__file__).resolve().parent
PARSER = Parser(Language(tree_sitter_cpp.language()))
FILES = subprocess.check_output(["git", "ls-files"], cwd=ROOT, text=True).splitlines()
SOURCE_SUFFIXES = {".cpp", ".h", ".hpp", ".c", ".inl"}
PLATFORM = {
    "D_PLATFORM_X360": 1, "CGS_PLATFORM_X360": 1, "_XBOX": 1, "XBOX": 1,
    "_XENON": 1, "_M_PPCBE": 1, "_M_PPC": 1,
    "WIN32": 0, "_WIN32": 0, "WIN64": 0, "_WIN64": 0, "_WINDOWS": 0,
    "D_PLATFORM_PC": 0, "D_PLATFORM_WIN32": 0, "D_PLATFORM_WIN64": 0,
    "D_PLATFORM_PS3": 0, "CGS_PLATFORM_PS3": 0, "_M_X64": 0, "_M_AMD64": 0,
    "_M_IX86": 0, "_M_IX86_FP": 0, "__SSE__": 0, "__SSE2__": 0,
    "__SSE3__": 0, "__SSE4_1__": 0, "__x86_64__": 0, "__i386__": 0,
}


def evaluate(expr):
    expr = re.sub(r"/\*.*?\*/|//.*", "", expr).strip()
    def defined(m):
        key = m.group(1) or m.group(2)
        return str(PLATFORM[key]) if key in PLATFORM else "UNKNOWN"
    expr = re.sub(r"defined\s*(?:\(\s*(\w+)\s*\)|(\w+))", defined, expr)
    for name, value in PLATFORM.items():
        expr = re.sub(r"\b" + re.escape(name) + r"\b", str(value), expr)
    expr = expr.replace("&&", " and ").replace("||", " or ")
    expr = re.sub(r"!(?!=)", " not ", expr)
    try:
        tree = ast.parse(expr.strip(), mode="eval")
    except SyntaxError:
        return None
    def walk(n):
        if isinstance(n, ast.Constant) and isinstance(n.value, (int, bool)):
            return n.value
        if isinstance(n, ast.UnaryOp) and isinstance(n.op, ast.Not):
            v = walk(n.operand)
            return None if v is None else not v
        if isinstance(n, ast.BoolOp):
            vals = [walk(v) for v in n.values]
            if isinstance(n.op, ast.And):
                return False if False in vals else (None if None in vals else all(vals))
            return True if True in vals else (None if None in vals else any(vals))
        if isinstance(n, ast.Compare) and len(n.ops) == 1:
            a, b = walk(n.left), walk(n.comparators[0])
            if a is None or b is None:
                return None
            op = n.ops[0]
            for cls, fn in [(ast.Eq, lambda: a == b), (ast.NotEq, lambda: a != b),
                            (ast.Gt, lambda: a > b), (ast.GtE, lambda: a >= b),
                            (ast.Lt, lambda: a < b), (ast.LtE, lambda: a <= b)]:
                if isinstance(op, cls):
                    return fn()
        return None
    v = walk(tree.body)
    return None if v is None else bool(v)


def specialize(text):
    lines = text.splitlines(keepends=True)
    def block(i, stops=()):
        result = []
        while i < len(lines):
            m = re.match(r"\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)", lines[i])
            if not m:
                result.append(lines[i]); i += 1; continue
            word, expr = m.groups()
            if word in stops:
                return "".join(result), i
            if word not in ("if", "ifdef", "ifndef"):
                raise RuntimeError(f"unbalanced preprocessor at {i + 1}")
            branches = []
            while True:
                directive = lines[i]
                if word == "ifdef": expr = "defined(" + expr.strip() + ")"
                if word == "ifndef": expr = "!defined(" + expr.strip() + ")"
                val = True if word == "else" else evaluate(expr)
                body, j = block(i + 1, ("elif", "else", "endif"))
                branches.append((directive, body, val))
                if j >= len(lines):
                    raise RuntimeError(f"unterminated preprocessor at {i + 1}")
                m = re.match(r"\s*#\s*(elif|else|endif)\b(.*)", lines[j])
                word, expr = m.groups()
                i = j
                if word == "endif": break
            # Only fold a chain when its selection is fully determined. Unknown
            # feature and include guards remain exactly as written.
            selected = None
            certain = True
            for _, body, val in branches:
                if val is None:
                    certain = False; break
                if val:
                    selected = body; break
            if certain:
                result.append(selected or "")
            else:
                result.extend(d + b for d, b, _ in branches)
                result.append(lines[i])
            i += 1
        return "".join(result), i
    return block(0)[0]


def owned(name):
    return name.startswith("src/") or name.startswith("vendor/renderware/")


def excluded(name):
    return name.startswith("src/pc/")


def host_file(name):
    p = Path(name)
    if excluded(name): return False
    if any(part.lower() == "pc" for part in p.parts): return True
    if re.search(r"PC\d*$", p.stem): return True
    if "_embed_check" in p.stem: return True
    if p.stem in {"CgsHarnessSlot", "BrnDirectorHarness", "BrnAIHarnessPad",
                   "BrnHarnessWinTeleport", "BrnDiagBoundSurfaces", "BrnDiagFilmLatch",
                   "BrnDiagTrailHeight", "CgsStreamDiag", "CgsDMixDiag",
                   "BrnSimpleFxDiag", "BrnHudSoundDiag", "BrnAISoundDiag",
                   "BrnEngineAudioDiag", "BrnEnvironmentSoundDiag", "BrnTrafficSoundDiag",
                   "BrnTrafficPvsDiag", "VolumeQueryHostLayout", "BrnMain",
                   "CgsFrameInterpolation", "CgsSoundPcmTrace", "ParticleModuleBringUp"}:
        return True
    return False


def source_files():
    return [name for name in FILES if owned(name) and not excluded(name)
            and Path(name).suffix.lower() in SOURCE_SUFFIXES and (ROOT / name).exists()]


def phase_one():
    removed, folded = [], []
    for name in FILES:
        if not owned(name): continue
        p = ROOT / name
        if host_file(name) and p.is_file():
            p.unlink(); removed.append(name)
    for name in source_files():
        p = ROOT / name
        raw = p.read_bytes()
        text = raw.decode("utf-8-sig")
        result = specialize(text)
        # XDK exposes the console's Win32-compatible kernel APIs through xtl.h.
        result = re.sub(r"(?im)^(\s*#\s*include\s*)[<\"]windows\.h[>\"]", r"\1<xtl.h>", result)
        if result != text:
            p.write_bytes((b"\xef\xbb\xbf" if raw.startswith(b"\xef\xbb\xbf") else b"") + result.encode("utf-8"))
            folded.append(name)
    removed = sorted({name for name in FILES if owned(name) and not (ROOT / name).exists()})
    (OUT / "phase_one.json").write_text(json.dumps({"removed": removed, "specialized": folded}, indent=2))
    print(f"Removed {len(removed)} host/scaffolding files; specialized {len(folded)} files")


def walk(node):
    yield node
    for child in node.named_children:
        yield from walk(child)


def host_identifier(name):
    if name in {"CgsPC", "BrnDiag", "HarnessSlot", "BrnDirectorHarness", "FrameProfile",
                "RestoreTickRenderPose", "LatchTickRenderPose", "ApplyRenderPoseInterpolation",
                "ResetRenderPoseInterpolation", "AptLifeLog", "gAptResourceSpanBase", "gAptResourceSpanSize"}:
        return True
    if "PoseTrack" in name or "Native8" in name:
        return True
    if name.startswith(("_mm_", "__m128", "__m256", "_udiv128", "_umul128")):
        return True
    if name.startswith("PC") and len(name) > 2 and (name[2].isupper() or name[2] == "_"):
        return name not in {"PCM", "PCWSTR", "PCSTR", "PCHAR"}
    if name.startswith(("KA", "KPC", "KAPC")) and name.isupper() and "_PC_" not in name:
        return False
    if "BringUp" in name or name.startswith(("BrnDiag_", "BrnHarness", "brn_diag_")):
        return True
    if re.search(r"^[a-z]+Pc[A-Z]|_PC\b|PC_[A-Za-z]", name):
        return True
    if name.startswith("AptHook_") or re.search(r"^Apt\w*Lock_(?:Acquire|Release)$", name):
        return True
    if re.search(r"(?:Diag(?:Enabled|Stream|Instance|Clock)|Witness(?:Enabled|Stream)|DiagnosticsPC)", name):
        return True
    return bool(re.search(r"(?:[a-z0-9_]PC(?:$|_)|_PC(?:$|_)|PCLeaf|^IsPC[A-Z])", name))


def text_of(node, data):
    return data[node.start_byte:node.end_byte].decode("utf-8")


def declarator_names(node, data):
    if node.type in {"namespace_definition", "struct_specifier", "class_specifier", "enum_specifier"}:
        name = node.child_by_field_name("name")
        return [text_of(name, data)] if name else []
    names = []
    for i, child in enumerate(node.children):
        if node.field_name_for_child(i) != "declarator":
            continue
        while child:
            if child.type in {"identifier", "field_identifier", "type_identifier",
                              "qualified_identifier", "operator_name", "destructor_name"}:
                names.append(text_of(child, data)); break
            child = child.child_by_field_name("declarator")
    return names


def erase_span(node, data, comments=False):
    start, end = node.start_byte, node.end_byte
    if comments:
        prev = node.prev_named_sibling
        while prev and prev.type == "comment" and not data[prev.end_byte:start].strip():
            prev_line = data.rfind(b"\n", 0, prev.start_byte) + 1
            if data[prev_line:prev.start_byte].strip():
                break
            start = prev.start_byte
            prev = prev.prev_named_sibling
    # C++ class/namespace nodes do not always own their terminating semicolon.
    if data[end:end + 1] == b";": end += 1
    line_start = data.rfind(b"\n", 0, start) + 1
    if not data[line_start:start].strip(): start = line_start
    line_end = data.find(b"\n", end)
    if line_end != -1 and not data[end:line_end].strip(): end = line_end + 1
    return start, end


def apply_spans(data, spans):
    merged = []
    for start, end in sorted(spans):
        if merged and start <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(end, merged[-1][1]))
        else:
            merged.append((start, end))
    for start, end in reversed(merged): data = data[:start] + data[end:]
    return data


def next_code(node):
    node = node.next_named_sibling
    while node and node.type == "comment": node = node.next_named_sibling
    return node


def marker_target(comment, data):
    # A trailing comment belongs to the preceding statement/field, not the next one.
    prev = comment.prev_named_sibling
    if prev and prev.end_point.row == comment.start_point.row:
        return prev
    parent = comment.parent
    if parent.type == "preproc_include": return None
    if parent.type not in {"translation_unit", "declaration_list", "field_declaration_list",
                           "compound_statement", "preproc_if", "preproc_ifdef", "preproc_else"}:
        if parent.type in {"initializer_list", "argument_list", "field_initializer_list"}:
            return None
        while parent and parent.type not in {"expression_statement", "declaration", "field_declaration",
                                              "return_statement", "if_statement", "function_definition"}:
            parent = parent.parent
        if parent: return parent
    target = next_code(comment)
    if not target: return None
    # A file-level description is not a license to remove an unrelated namespace.
    if target.type in {"preproc_include", "namespace_definition", "access_specifier", "preproc_def"}:
        return None
    return target


def marker_plan():
    plan = []
    for name in source_files():
        data = (ROOT / name).read_bytes()
        for node in walk(PARSER.parse(data).root_node):
            if node.type != "comment" or not re.search(rb"FLAG PC|PC-platform leaf", data[node.start_byte:node.end_byte], re.I):
                continue
            target = marker_target(node, data)
            plan.append({"file": name, "line": node.start_point.row + 1,
                         "marker": text_of(node, data).strip(),
                         "kind": target.type if target else "MANUAL",
                         "target": text_of(target, data).splitlines()[0][:160] if target else ""})
    (OUT / "marker_plan.json").write_text(json.dumps(plan, indent=2), encoding="utf-8")
    print(collections.Counter(x["kind"] for x in plan))
    for item in plan:
        if item["kind"] in {"MANUAL", "function_definition", "template_declaration", "class_specifier", "struct_specifier"}:
            print(f'{item["file"]}:{item["line"]} {item["kind"]} {item["target"]} | {item["marker"]}')


DECLARATIONS = {"declaration", "field_declaration", "function_definition", "class_specifier",
                "struct_specifier", "enum_specifier", "namespace_definition", "alias_declaration",
                "type_definition", "friend_declaration", "using_declaration", "namespace_alias_definition"}
STATEMENTS = {"expression_statement", "return_statement", "if_statement", "while_statement",
              "for_statement", "for_range_loop", "switch_statement", "declaration"}
IDENTIFIERS = {"identifier", "field_identifier", "type_identifier", "namespace_identifier"}


def containing(node, kind):
    while node:
        if node.type == kind: return node
        node = node.parent
    return None


def condition_without_host(node, data):
    condition = node.child_by_field_name("condition")
    if not condition: return None
    value = text_of(condition, data)
    value = re.sub(r"\b(?:[A-Za-z_]\w*::)*(?:IsPreparedPC|HasProgramsPC|IsGraphicsLoadedBringUp|IsRaceCarLoadedForStateMachineBringUp)\([^()]*\)", "1", value)
    # Host-only feature predicates are absent on console. Preserve the native
    # alternative when it is explicit in the source.
    value = re.sub(r"\b(?:[A-Za-z_]\w*::)*(?:IsVehicleCapture|IsShadowCapture)\([^()]*\)", "0", value)
    for token in set(re.findall(r"\b[A-Za-z_]\w*", value)):
        if host_identifier(token):
            value = re.sub(r"\b" + re.escape(token) + r"\b\s*\([^()]*\)", "0", value)
            value = re.sub(r"\b" + re.escape(token) + r"\b", "0", value)
    value = re.sub(r"\b(?:[A-Za-z_]\w*::)+(0|1)\b", r"\1", value)
    value = re.sub(r"\b(?:[A-Za-z_]\w*\s*(?:->|\.))+(0|1)\b", r"\1", value)
    return evaluate(value)


def phase_two(dry_run=False, symbols_only=False):
    phase_one_info = json.loads((OUT / "phase_one.json").read_text())
    removed_paths = {name for name in FILES if owned(name) and not (ROOT / name).exists()}
    removed_basenames = {Path(n).name for n in removed_paths}
    records = []
    changed = 0
    for name in source_files():
        path = ROOT / name
        data = path.read_bytes()
        root = PARSER.parse(data).root_node
        spans = []
        replacements = []
        def remove(node, reason, comments=False):
            # Removing the only controlled statement must also remove its control
            # wrapper; otherwise the next native statement would become its body.
            while node.parent:
                parent = node.parent
                if parent.type in {"template_declaration", "condition_clause", "init_statement"}:
                    node = parent
                elif parent.type == "else_clause":
                    node = parent
                elif parent.type in {"if_statement", "while_statement", "for_statement", "switch_statement"} and parent.child_by_field_name("condition") == node:
                    node = parent
                elif parent.type == "if_statement" and parent.child_by_field_name("consequence") == node:
                    alternate = parent.child_by_field_name("alternative")
                    if alternate:
                        body = alternate.named_children[-1] if alternate.type == "else_clause" else alternate
                        replacements.append((parent.start_byte,parent.end_byte,data[body.start_byte:body.end_byte]))
                        return
                    node = parent
                elif parent.type in {"while_statement","for_statement","for_range_loop","do_statement"} and parent.child_by_field_name("body") == node:
                    node = parent
                else:
                    break
            start, end = erase_span(node, data, comments)
            spans.append((start, end))
            records.append({"file":name,"line":node.start_point.row+1,"kind":node.type,
                            "reason":reason,"text":text_of(node,data).splitlines()[0][:180]})
        for node in walk(root):
            if node.type == "preproc_include":
                include = re.search(r'[<\"]([^>\"]+)[>\"]', text_of(node, data))
                if include:
                    target = include.group(1).replace("\\", "/")
                    if (target.lower().startswith("pc/") or "/pc/" in target.lower()
                        or target in removed_paths or Path(target).name in removed_basenames
                        or Path(target).name.lower() in {"xmmintrin.h","emmintrin.h","immintrin.h","smmintrin.h","nmmintrin.h","tmmintrin.h","pmmintrin.h","mmintrin.h"}):
                        remove(node,"host include")
                continue
            if node.type in DECLARATIONS:
                declared = declarator_names(node,data)
                if any(any(host_identifier(t) for t in re.findall(r"\b[A-Za-z_]\w*", n)) for n in declared):
                    remove(node,"host declaration",True)
                    continue
            if node.type in IDENTIFIERS and host_identifier(text_of(node,data)):
                # The smallest complete statement keeps neighboring recovered code.
                parent = node.parent
                while parent and parent.type not in STATEMENTS | DECLARATIONS:
                    parent = parent.parent
                if not parent: continue
                if parent.type == "if_statement":
                    decision = condition_without_host(parent,data)
                    consequent = parent.child_by_field_name("consequence")
                    alternate = parent.child_by_field_name("alternative")
                    if decision is True and consequent:
                        replacements.append((parent.start_byte,parent.end_byte,data[consequent.start_byte:consequent.end_byte]))
                        records.append({"file":name,"line":parent.start_point.row+1,"kind":"if_statement","reason":"retain console arm","text":text_of(parent,data).splitlines()[0][:180]})
                    elif alternate:
                        body = alternate.named_children[-1] if alternate.type == "else_clause" else alternate
                        replacements.append((parent.start_byte,parent.end_byte,data[body.start_byte:body.end_byte]))
                        records.append({"file":name,"line":parent.start_point.row+1,"kind":"if_statement","reason":"retain console alternative","text":text_of(parent,data).splitlines()[0][:180]})
                    else:
                        remove(parent,"host condition")
                elif parent.type == "return_statement":
                    fn = containing(parent,"function_definition")
                    if fn: remove(fn,"host-dependent return",True)
                elif parent.type in {"for_statement","while_statement","switch_statement","for_range_loop"}:
                    remove(parent,"host control flow")
                else:
                    remove(parent,"host reference")
            if node.type != "comment" or symbols_only: continue
            marker = text_of(node,data)
            if not re.search(r"FLAG PC|PC-platform leaf",marker,re.I): continue
            # These are historical descriptions of missing console work, not host
            # statements. Do not discard the recovered function following them.
            if re.search(r"CORRECTED|was FLAG|above|below|banner|see |dropped|not reproduced|not bodied|NOT copied|not reconstructed|un.homed|absent|elided|NO.OP|only the dispatch|THE DISPATCH|pointer widening",marker,re.I):
                continue
            target = marker_target(node,data)
            if target is None: continue
            if target.type in {"function_definition","template_declaration"}:
                declared = declarator_names(target,data)
                code = text_of(target,data)
                explicit_host = re.search(r"no console (?:body|counterpart)|no counterpart|NOT (?:an? )?X360|NOT IN THE (?:X360|CONSOLE)|fixed.function|CPU fold|host.*(?:shim|only)|thread.id plumbing|interlocked TAS|spinlock|single.threaded|host QPC|native codec|D3D upload|vtable filler|native invocation",marker,re.I)
                if explicit_host or any(host_identifier(x) for n in declared for x in re.findall(r"\w+",n)):
                    remove(target,"marked host implementation",True)
            elif target.type in {"expression_statement","declaration","field_declaration","if_statement",
                                 "compound_statement","for_statement","return_statement","friend_declaration",
                                 "using_declaration","class_specifier","struct_specifier","alias_declaration"}:
                # A compound function body is the whole implementation; remove its
                # definition instead of silently turning it into a declaration.
                if target.type == "compound_statement" and target.parent.type == "function_definition":
                    remove(target.parent,"marked host implementation",True)
                else: remove(target,"marked host block",True)
        # Apply outermost edits first; nested edits are already covered by them.
        edits = [(a,b,b"") for a,b in spans] + replacements
        accepted = []
        for a,b,replacement in sorted(edits,key=lambda e:(e[0],-e[1])):
            if accepted and a < accepted[-1][1]: continue
            accepted.append((a,b,replacement))
        output = data
        for a,b,replacement in reversed(accepted): output=output[:a]+replacement+output[b:]
        if output != data:
            changed += 1
            if not dry_run: path.write_bytes(output)
    (OUT / ("symbols.json" if symbols_only else ("phase_two_plan.json" if dry_run else "phase_two.json"))).write_text(json.dumps(records,indent=2),encoding="utf-8")
    print(f'{"Would edit" if dry_run else "Edited"} {changed} files; {len(records)} host ranges')
    print(collections.Counter(x["reason"] for x in records))


if __name__ == "__main__":
    if "--plan" in sys.argv: marker_plan()
    elif "--strip" in sys.argv: phase_two("--dry-run" in sys.argv, "--symbols-only" in sys.argv)
    else: phase_one()
