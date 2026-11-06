from pathlib import Path
import ast,re,json,hashlib

ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
parser=Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-external-body-root/remove_pc_probes.py')
exec(compile(ast.Module(body=[n for n in ast.parse(parser.read_text()).body if isinstance(n,ast.FunctionDef)],type_ignores=[]),'brace_parser','exec'))
paths=['src/GameSource/GameState/Offences/StuntManager_gUI_00.cpp','src/GameSource/GameState/Offences/StuntManager_gUI_01.cpp']
before={p:(ROOT/p).read_text()for p in paths}
s=before[paths[0]]
s=cut_block(s,'    // [DIAG] NOT IN THE X360 BINARY. Same env guard + logger','bool UIGateDiagFirstN(')
s=cut_block(s,'    // [DIAG] NOT IN THE X360 BINARY. The [stunt-boost]','if (!lbIsOnline)')
s=cut_block(s,'            static s32        siTipDiagLines','if (sbCollectDiag')
s=cut_block(s,'        static s32 siRepeatDiagCount','if (UIGateDiagFirstN(')
s=cut_block(s,'    // [DIAG] NOT IN THE X360 BINARY. Same env guard + first-N budget','if (UIGateDiagFirstN(')
for marker in ['    static s32 siCompleteDiagCount','            static s32 siCountyDiagCount','        static s32 siAllDiagCount']:
    s=cut_block(s,marker,'if (UIGateDiagFirstN(')
assert 'if (lbPostedBoostAction)'in s,'native boost guard changed'
repeat=s[s.index('    if (lbAlreadyDone)'):s.index('    if (!mpProgressionManager)',s.index('    if (lbAlreadyDone)'))]
assert 'return;' in repeat and not any(x in repeat for x in ['UIGateDiag','gpDebugPrint']),'native repeat return changed'
after={paths[0]:s}
s=before[paths[1]]
s=cut_block(s,'    static s32        siCompleteAllDiagLines','if (sbCollectDiag')
after[paths[1]]=s
for p,s in after.items():
    s=re.sub(r'^#include <stdlib.h>[^\n]*\n','',s,flags=re.M)
    s=re.sub(r'^#include [^\n]*CgsLog.h[^\n]*\n','',s,flags=re.M)
    assert not any(t in s for t in ['sbCollectDiag','UIGateDiagFirstN','gpDebugPrint']),p
    after[p]=s
for p,s in after.items():
    q=HERE/('before-'+Path(p).name);assert not q.exists();q.write_text(before[p],encoding='utf-8')
for p,s in after.items():(ROOT/p).write_text(s,encoding='utf-8')
sha=lambda s:hashlib.sha256(s.encode()).hexdigest()
receipt={p:{'before_sha256':sha(before[p]),'after_sha256':sha(after[p]),'lines_removed':len(before[p].splitlines())-len(after[p].splitlines())}for p in paths}
(HERE/'stunt-pc-removal.json').write_text(json.dumps({'paths':receipt,'scope':'Only added PC diagnostic helpers/branches/counters and unused host headers removed. Native boost condition, repeat earlyreturn, eventposts, progression and training calls retained.'},indent=2)+'\n',encoding='utf-8')
print(json.dumps({'removed_lines':sum(v['lines_removed']for v in receipt.values())}))
