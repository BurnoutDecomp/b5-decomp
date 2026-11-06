from pathlib import Path
import sys


here = Path(__file__).resolve().parent
source = Path(sys.argv[1]) / "src/SDKs/EATech/include/Apt"
prefix = '#include <xtl.h>\n#include "GameShared/GameClasses/Core/CgsStringUtils.h"\n'


def replace(body, anchor, replacement):
    assert body.count(anchor) == 1, anchor
    return body.replace(anchor, replacement)


def log(marker, statement, indent):
    return (
        f'{indent}if (gbR104ProbeMember) OutputDebugStringA("R104_{marker}_BEGIN\\n");\n'
        f'{statement}\n'
        f'{indent}if (gbR104ProbeMember) OutputDebugStringA("R104_{marker}_END\\n");'
    )


name = "AptActionInterpreterStackOps.cpp"
body = (source / name).read_text(encoding="utf-8")
start = body.index("void AptActionInterpreter::_FunctionAptActionStringDictByteGetMember(")
end = body.index("void AptActionInterpreter::_FunctionAptActionStringDictByteGetVar(", start)
before, af, after = body[:start], body[start:end], body[end:]
anchor = "    const unsigned int nIndex = *pCtx->mpProgramCounter;    // byte dictionary index"
af = replace(
    af, anchor,
    """    const bool lbPreviousProbe = gbR104ProbeMember;
    gbR104ProbeMember = gbTraceAptRuntime;
""" + anchor + """
    if (gbR104ProbeMember)
    {
        char lacProbe[150];
        CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
            "R104_AF pc=%08X index=%u top=%d object=%08X\\n",
            reinterpret_cast<u32>(pCtx->mpProgramCounter - 1), nIndex,
            pInterp->mnStackTop, pInterp->mnStackTop > 0
                ? reinterpret_cast<u32>(pInterp->mpStack[pInterp->mnStackTop - 1]) : 0u);
        OutputDebugStringA(lacProbe);
    }""",
)
anchor = "    pEntry->AddRef();"
af = replace(af, anchor, log("AF_ADDREF", anchor, "    "))
anchor = "    _FunctionAptActionGetMember(pInterp, pCtx);"
af = replace(
    af, anchor,
    log("AF_GETMEMBER", anchor, "    ") + "\n    gbR104ProbeMember = lbPreviousProbe;",
)
with (here / name).open("x", encoding="utf-8") as output:
    output.write(prefix + "extern bool gbTraceAptRuntime;\nbool gbR104ProbeMember = false;\n"
                 + before + af + after)

name = "AptActionInterpreterMemberOps.cpp"
body = (source / name).read_text(encoding="utf-8")
start = body.index("void AptActionInterpreter::_FunctionAptActionGetMember(")
end = body.index("void AptActionInterpreter::_FunctionAptActionSetMember(", start)
before, get, after = body[:start], body[start:end], body[end:]
anchor = "        const AptVirtualFunctionTable_Indices eKey = pKey->getVtblIndex();"
get = replace(
    get, anchor,
    anchor + """
        if (gbR104ProbeMember)
        {
            char lacProbe[150];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R104_MEMBER_TYPES object=%08X type=%d key=%08X keyType=%d\\n",
                reinterpret_cast<u32>(pObject), static_cast<s32>(eObj),
                reinterpret_cast<u32>(pKey), static_cast<s32>(eKey));
            OutputDebugStringA(lacProbe);
        }""",
)
anchor = "        const EAStringC* pName = AptValue::Get_ToString(pKey, &scratch);"
get = replace(
    get, anchor,
    log("MEMBER_TOSTRING", anchor, "        ") + """
        if (gbR104ProbeMember)
        {
            char lacProbe[140];
            CgsCore::SPrintf(lacProbe, sizeof(lacProbe),
                "R104_MEMBER_NAME %.48s\\n", pName->GetBuffer());
            OutputDebugStringA(lacProbe);
        }""",
)
for marker, anchor in (
    ("MEMBER_VARIABLE", "        AptValue* pResult = pInterp->getVariable(pObject, 0, pName, 1, 0, 1);"),
    ("MEMBER_POP", "        pInterp->stackPopAndPush(2, pResult);"),
):
    get = replace(get, anchor, log(marker, anchor, "        "))
with (here / name).open("x", encoding="utf-8") as output:
    output.write(prefix + "extern bool gbR104ProbeMember;\n" + before + get + after)

name = "AptActionInterpreterVariable.cpp"
body = (source / name).read_text(encoding="utf-8")
for marker, anchor, indent in (
    ("VARIABLE_LOOKUP", "            pFound = pContext->objectMemberLookup(pContext, pLeaf);", "            "),
    ("VARIABLE_CHILD", "                pFound = pContext->findChild(pLeaf, pTgt);                 // homed (AptValueFindChild.cpp)", "                "),
    ("VARIABLE_FALLBACK", "                pFound = LookupGlobalFallback(pContext, pLeaf, nDirect);  // the _level / global-frame fallback", "                "),
):
    body = replace(body, anchor, log(marker, anchor, indent))
anchor = """                    if (gpAptVarNotFoundCb)
                        gpAptVarNotFoundCb(pName->GetBuffer());"""
body = replace(
    body, anchor,
    "                    if (gpAptVarNotFoundCb)\n                    {\n"
    + log("VARIABLE_NOTFOUND", "                        gpAptVarNotFoundCb(pName->GetBuffer());", "                        ")
    + "\n                    }",
)
with (here / name).open("x", encoding="utf-8") as output:
    output.write(prefix + "extern bool gbR104ProbeMember;\n" + body)
