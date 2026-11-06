import pathlib
import re
import subprocess

root = pathlib.Path.cwd()
src = "src/SDKs/EATech/"
apt = src + "Apt/"
headers = src + "include/Apt/"
string_pool = headers + "AptString/StringPool.h"
apt_init = apt + "AptInit.cpp"
globals_file = src + "AptGlobals.cpp"
stack = headers + "AptActionInterpreterStackOps.cpp"

slots = {
    "GLOBAL": (7, "_global"), "ARRAY": (31, "array"), "BOOLEAN": (34, "boolean"),
    "COLOR": (36, "color"), "CONTROLLER": (37, "controller"), "DATE": (38, "date"),
    "ERROR": (39, "error"), "FUNCTION": (41, "function"), "MOVIECLIP": (48, "movieclip"),
    "NULL": (50, "null"), "NUMBER": (51, "number"), "OBJECT": (52, "object"),
    "PROTOTYPE": (70, "prototype"), "STRING": (77, "string"), "SUPER": (78, "super"),
    "TEXTFORMAT": (79, "textformat"), "THIS": (80, "this"), "UNDEFINED": (82, "undefined"),
    "XML": (85, "XML"),
}
aliases = {
    "gAptKeyThis": "THIS", "gAptThisKey": "THIS", "gAptKeyGlobal": "GLOBAL",
    "gAptKeySuper": "SUPER", "gAptKeyPrototype": "PROTOTYPE", "gAptKeyControllerKey": "CONTROLLER",
    "gAptEmptyMethodName": "SUPER", "gAptObjectClassName": "OBJECT",
    "gAptStringClassName": "STRING", "gAptSpriteClassKey": "MOVIECLIP",
    "gAptTypeName_Undefined": "UNDEFINED", "gAptTypeName_Number": "NUMBER",
    "gAptTypeName_Boolean": "BOOLEAN", "gAptTypeName_String": "STRING",
    "gAptTypeName_Object": "OBJECT", "gAptTypeName_MovieClip": "MOVIECLIP",
    "gAptTypeName_Null": "NULL", "gAptTypeName_Function": "FUNCTION",
}

def load(path):
    raw = (root / path).read_bytes()
    return raw.decode("utf-8").replace("\r\n", "\n"), b"\r\n" in raw

def save(path, data, crlf):
    (root / path).write_bytes((data.replace("\n", "\r\n") if crlf else data).encode("utf-8"))

records = re.findall(r'\{\{1, \d+, 1, 0\}, "([^"]*)"\}', load(apt + "AptStringPool.cpp")[0])
assert len(records) == 88, len(records)
for name, (index, literal) in slots.items():
    assert records[index] == literal, (name, index, records[index], literal)

text, style = load(string_pool)
line = "    enum { KU_CONSTANT_COUNT = 88 };\n"
assert text.count(line) == 1
enum = ["    // Constant codes used by the native class bootstrap and name aliases.",
        "    enum ConstantName", "    {"]
enum += [f"        KU_NAME_{name} = {index}," for name, (index, _) in slots.items()]
enum += ["    };", ""]
save(string_pool, text.replace(line, line + "\n" + "\n".join(enum), 1), style)

text, style = load(globals_file)
start = text.index("// 8. Interned AS-name string constants")
stop = text.index("// ===========================================================================\n// 9. Name", start)
body = ["// 8. Named views of ARTIST's interned AS strings. StringPool::Initialize",
        "//    publishes their backing records after the Apt allocator is ready.",
        "// ==========================================================================="]
for name, slot in aliases.items():
    address = 0x8324E580 + slots[slot][0] * 4
    body.append(f"extern const EAStringC& {name} = StringPool::saConstant[StringPool::KU_NAME_{slot}]; // {address:08X}")
body += ["", "const EAStringC* gpAptThisKey = &StringPool::saConstant[StringPool::KU_NAME_THIS];", "", ""]
text = text[:start] + "\n".join(body) + text[stop:]
include = '#include "SDKs/EATech/include/Apt/AptString/StringPool.h"'
if include not in text:
    first = text.index("#include ")
    text = text[:first] + include + "\n" + text[first:]
save(globals_file, text, style)

text, style = load(apt_init)
start = text.index("extern const EAStringC gAptObjectClassName;")
stop = text.index("// off_8324E378", start)
text = text[:start] + text[stop:]
text = text.replace("pGlobals->Lookup(gAptObjectClassName)",
                    "pGlobals->Lookup(*StringPool::GetString(StringPool::KU_NAME_OBJECT))")
start = text.index("            // The nine-entry builtin class table")
stop = text.index("            for (int li = 0; li < 9; ++li)", start)
ordered = ("OBJECT", "ARRAY", "COLOR", "DATE", "TEXTFORMAT", "MOVIECLIP", "XML", "ERROR", "STRING")
body = ["            // ARTIST 0x82AF6B68 uses the initialized StringPool slots directly.",
        "            const EAStringC* const lakNames[9] =", "            {"]
body += [f"                StringPool::GetString(StringPool::KU_NAME_{name})," for name in ordered]
body += ["            };", ""]
text = text[:start] + "\n".join(body) + text[stop:]
save(apt_init, text, style)

pattern = re.compile(r"(extern\s+const\s+EAStringC)(\s+)(" + "|".join(aliases) + r")\s*;")
files = subprocess.check_output(["git", "ls-files", "src"], text=True).splitlines()
for path in files:
    if path in {apt_init, globals_file, stack} or not path.endswith((".cpp", ".h", ".hpp")):
        continue
    raw = (root / path).read_bytes()
    if b"extern const EAStringC" not in raw:
        continue
    text, style = load(path)
    text, changes = pattern.subn(r"\1&\2\3;", text)
    if changes:
        save(path, text, style)
