import hashlib, json, pathlib, subprocess
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
assignment=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\r19-strict-game-link-diagnostic\unresolved-disjoint-assignments.json')
base='6be513e1b3b69f9c28ed42a01ffafdc92924f857'
rows=json.loads(assignment.read_text(encoding='utf-8'))
maybe=next(x['callers'] for x in rows['lanes']['02_network_dirtysdk'] if 'MaybeDrawText' in x['symbol'])
paths=['src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.cpp','src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.h','src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidget.cpp','src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Widgets/CgsWidgetLineGraph.h','src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.cpp']+maybe
entries=[]
for rel in paths:
    p=repo/rel
    exists=p.exists()
    raw=p.read_bytes() if exists else None
    try:
        blob=subprocess.check_output(['git','-C',str(repo),'rev-parse',base+':'+rel],stderr=subprocess.DEVNULL,text=True).strip()
    except subprocess.CalledProcessError:
        blob=None
    entries.append({'path':rel,'exists_pre_edit':exists,'git_blob_at_base':blob,'raw_sha256_pre_edit':hashlib.sha256(raw).hexdigest() if raw is not None else None,'bytes_pre_edit':len(raw) if raw is not None else None})
result={'base':base,'branch':'codex/experimental-x360-r19-debug-providers-luna','compiler_invoked_before_capture':False,'files':entries}
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\debug-providers-luna\proof\SCOPE_PRE_EDIT.json')
out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('file_count=%d' % len(entries))
print('new_files=%s' % ','.join(x['path'] for x in entries if not x['exists_pre_edit']))
print('manifest=%s' % out)
