import hashlib, json, pathlib, re
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
assign=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\r19-strict-game-link-diagnostic\unresolved-disjoint-assignments.json')
rows=json.loads(assign.read_text(encoding='utf-8'))
paths=next(x['callers'] for x in rows['lanes']['02_network_dirtysdk'] if 'MaybeDrawText' in x['symbol'])
pattern=re.compile(rb'(?m)^([ \t]*)int([ \t]+MaybeDrawText[ \t]*\()')
results=[]
for rel in paths:
    p=repo/rel; data=p.read_bytes(); updated,count=pattern.subn(rb'\1void\2',data)
    if count!=1: raise SystemExit('%s: expected exactly 1 declaration, found %d' % (rel,count))
    p.write_bytes(updated)
    results.append({'path':rel,'prototype_count_changed':count,'pre_sha256':hashlib.sha256(data).hexdigest(),'post_sha256':hashlib.sha256(updated).hexdigest()})
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\debug-providers-luna\proof\MAYBE_PROTOTYPE_CLEANUP.json')
out.write_text(json.dumps({'caller_files':len(results),'change':'int MaybeDrawText prototype return changed to void; parameter list retained','files':results},indent=2)+'\n',encoding='utf-8')
print('caller_files=%d' % len(results))
print('prototype_updates=%d' % sum(x['prototype_count_changed'] for x in results))
print('receipt='+str(out))
