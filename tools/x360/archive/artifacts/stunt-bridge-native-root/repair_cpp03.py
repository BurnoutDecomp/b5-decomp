from pathlib import Path
import ast,re,json,hashlib
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/external-body-native-root')
HERE=Path(__file__).resolve().parent
p=ROOT/'src/GameSource/Game/GameBridgeGameStateToX_StuntGuiEvents.cpp'
before=p.read_text();s=before
helper=Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-external-body-root/remove_pc_probes.py')
exec(compile(ast.Module(body=[n for n in ast.parse(helper.read_text()).body if isinstance(n,ast.FunctionDef)],type_ignores=[]),str(helper),'exec'))
pattern=r'const auto([&*])(\s*const)?\s+(\w+)(\s*=\s*\*?reinterpret_cast<const\s+([^;]+?)\s*\*>\s*\(\s*lpAction\s*\);)'
def explicit(m):
    ty=' '.join(m[5].split())
    return 'const '+ty+m[1]+(' const' if m[2] else '')+' '+m[3]+m[4]
s,n=re.subn(pattern,explicit,s);assert n==44,n
assert s.count('const auto* scoring =')==1
s=s.replace('const auto* scoring =','const BrnGameState::GameStateModuleIO::ScoringOutputInterface* scoring =')
assert not re.search(r'\bauto\b',s)
zero=r'(\b(BrnGui::[A-Za-z_0-9]+)\s+[A-Za-z_0-9]+\s*=\s*)\{\};'
s,nvalue=re.subn(zero,lambda m:m[1]+m[2]+'();',s)

for marker,token in [
 ('                static s32        siDiscoLinesLeft','if (sbDiscoDiag'),
 ('                // [DIAG] NOT IN THE BINARY -- the takedown HUD chain','if (CgsDev::Log::gpDebugPrint'),
 ('                // [DIAG] NOT IN THE X360 BINARY -- the district chain','if (CgsDev::Log::gpDebugPrint'),
 ('                        // [DIAG] NOT IN THE X360 BINARY -- the [tut-ticker]','if (siTickerDiagLeft'),
 ('                // [DIAG] NOT IN THE X360 BINARY. Unconditional','if (CgsDev::Log::gpDebugPrint'),
 ('                // [DIAG] NOT IN THE X360 BINARY -- the stat panel','if (CgsDev::Log::gpDebugPrint'),
 ('                // [DIAG] NOT IN THE X360 BINARY -- the licence ladder','if (CgsDev::Log::gpDebugPrint'),
 ('                // [DIAG] NOT IN THE X360 BINARY. BRN_SHOWTIME_SCORE_DIAG','{'),
 ('                static s32        siRoadRulesDiagLeft','if (sbRoadRulesDiag'),
 ('            // [DIAG] NOT IN THE X360 BINARY -- one line per publish','if (CgsDev::Log::gpDebugPrint'),
]:s=cut_block(s,marker,token)
s=re.sub(r'^\s*// \[DIAG\] BRN_TD_DIAG -- NOT IN THE X360 BINARY\.\n','\n',s,flags=re.M)
s=re.sub(r'^    // \[DIAG\] NOT IN THE X360 BINARY -- the race-mode HUD message rung[^\n]*\n    //[^\n]*\n    //[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^        // \[DIAG\] the `\[UI-gate\]`[^\n]*\n        //[^\n]*\n        static s32[^\n]*siDiagLinesLeft[^\n]*\n','',s,flags=re.M)
s=re.sub(r'^#include <stdlib.h>[^\n]*\n','',s,flags=re.M)
s,ntrack=re.subn(r'^\s*(?:s32 liPostedGuiEventId;|liPostedGuiEventId\s*= lEvent.GetEventType\(\);)\n','\n',s,flags=re.M);assert ntrack==3,ntrack

old='''                u8* lpEventBytes = reinterpret_cast<u8*>(&lEvent);
                std::memcpy(lpEventBytes, &lpPaybackMugshotAction->mRoadRuleBeatenRoadID, sizeof(CgsID));
                std::memcpy(lpEventBytes + 8, &lpPaybackMugshotAction->meMugshotResponse,
                            sizeof(lpPaybackMugshotAction->meMugshotResponse));
                std::memcpy(lpEventBytes + 12, &lpPaybackMugshotAction->meImageType,
                            sizeof(lpPaybackMugshotAction->meImageType));
                std::memcpy(lpEventBytes + 16, &liAggressorCarIndex, sizeof(liAggressorCarIndex));
                lpEventBytes[20] = static_cast<u8>(lbLocalAggressor);'''
new='''                lEvent.mRoadId = lpPaybackMugshotAction->mRoadRuleBeatenRoadID;
                lEvent.meResponse = lpPaybackMugshotAction->meMugshotResponse;
                lEvent.meImageType = lpPaybackMugshotAction->meImageType;
                lEvent.meOpponentARCI = static_cast<EActiveRaceCarIndex>(liAggressorCarIndex);
                lEvent.mbLocalAggressor = lbLocalAggressor;'''
assert s.count(old)==1;s=s.replace(old,new)
old='''                u8* lpEventBytes = reinterpret_cast<u8*>(&lEvent);
                std::memset(lpEventBytes, 0, sizeof(CgsID));
                std::memcpy(lpEventBytes + 8, &liResponse, sizeof(liResponse));
                std::memcpy(lpEventBytes + 12, &lpAbortCaptureAction->meImageType,
                            sizeof(lpAbortCaptureAction->meImageType));
                std::memcpy(lpEventBytes + 16, &liAggressorCarIndex, sizeof(liAggressorCarIndex));
                lpEventBytes[20] = static_cast<u8>(lbLocalAggressor);'''
new='''                lEvent.mRoadId = 0;
                lEvent.meResponse = static_cast<BrnGameState::GameStateModuleIO::EMugshotResponse>(liResponse);
                lEvent.meImageType = lpAbortCaptureAction->meImageType;
                lEvent.meOpponentARCI = static_cast<EActiveRaceCarIndex>(liAggressorCarIndex);
                lEvent.mbLocalAggressor = lbLocalAggressor;'''
assert s.count(old)==1;s=s.replace(old,new)
# ARTIST823EDD30 passes the pending record's frame-local address, not a static.
s=s.replace('''        // zeroed at entry; the case-45 arm APPENDS into it and the tail posts it ONCE, after the
        // loop, when the count is > 0. Static, not stack: the single-threaded build's oversized-
        // event-local convention (the world bridge's own per-frame 199 record does the same). Its
        // never-written tail bytes stay zero here where the console's frame local carried junk.
        static BrnGui::GuiEventUpdateSatNav lPendingSatNavEvent;''',
'''        // zeroed at entry; case45 appends into it and the tail posts it once when nonempty.
        BrnGui::GuiEventUpdateSatNav lPendingSatNavEvent;''')
assert not any(x in s for x in ['sbDiscoDiag','sbRoadRulesDiag','siDiagLinesLeft','siGateLines','liPostedGuiEventId'])
assert re.findall(r'^\s*case ([^\n:]+(?:::[^\n:]+)*):',before,re.M)==re.findall(r'^\s*case ([^\n:]+(?:::[^\n:]+)*):',s,re.M)
(HERE/'before.cpp').write_text(before,encoding='utf-8')
p.write_text(s,encoding='utf-8')
(HERE/'source-change.json').write_text(json.dumps({'source':str(p),'auto_casts_replaced':n,'scoring_pointer_typed':1,'value_initializations_preserved':nvalue,'before_sha256':hashlib.sha256(before.encode()).hexdigest(),'after_sha256':hashlib.sha256(s.encode()).hexdigest(),'limits':'Existing event layouts and zero-initialization semantics preserved except the explicitly native frame-local pending record. All real case arms retained; only labeled PC diagnostics/counters removed. No complete translator parity claim.'},indent=2)+'\n',encoding='utf-8')
print('explicit types',n+1,'value-init',nvalue,'lines',len(before.splitlines()),'to',len(s.splitlines()))
