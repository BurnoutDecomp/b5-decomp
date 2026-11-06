import json, pathlib, re
repo=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\native_debug_lanes_luna_129a')
assignment=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\r19-strict-game-link-diagnostic\unresolved-disjoint-assignments.json')
rows=json.loads(assignment.read_text(encoding='utf-8'))['lanes']['02_network_dirtysdk']
callers=next(x['callers'] for x in rows if 'MaybeDrawText' in x['symbol'])

def mask_comments_strings(src):
    out=list(src); i=0; state='normal'
    while i<len(src):
        if state=='normal':
            if src.startswith('//',i): out[i]=out[i+1]=' '; i+=2; state='line'; continue
            if src.startswith('/*',i): out[i]=out[i+1]=' '; i+=2; state='block'; continue
            if src[i]=='"': out[i]=' '; i+=1; state='string'; continue
            if src[i]=="'": out[i]=' '; i+=1; state='char'; continue
            i+=1; continue
        if state=='line':
            if src[i]=='\n': state='normal'
            else: out[i]=' '
            i+=1; continue
        if state=='block':
            if src.startswith('*/',i): out[i]=out[i+1]=' '; i+=2; state='normal'; continue
            if src[i]!='\n': out[i]=' '
            i+=1; continue
        if state in ('string','char'):
            delim='"' if state=='string' else "'"
            if src[i]=='\\' and i+1<len(src): out[i]=out[i+1]=' '; i+=2; continue
            if src[i]==delim: out[i]=' '; i+=1; state='normal'; continue
            if src[i]!='\n': out[i]=' '
            i+=1
    return ''.join(out)

def split_args(s):
    parts=[]; start=0; depth=0
    for i,ch in enumerate(s):
        if ch=='(': depth+=1
        elif ch==')': depth-=1
        elif ch==',' and depth==0: parts.append(s[start:i].strip()); start=i+1
    parts.append(s[start:].strip()); return parts

files=[]; total=0; violations=[]; bool_values={}
for rel in callers:
    raw=(repo/rel).read_text(encoding='utf-8-sig'); clean=mask_comments_strings(raw)
    void_declarations=len(re.findall(r'(?m)^\s*void\s+MaybeDrawText\s*\(',clean))
    int_declarations=len(re.findall(r'(?m)^\s*int\s+MaybeDrawText\s*\(',clean))
    starts=[m.start() for m in re.finditer(r'(?<![A-Za-z0-9_])(?:::)?MaybeDrawText\s*\(',clean)]
    calls=0
    for start in starts:
        line_start=clean.rfind('\n',0,start)+1
        if re.search(r'\b(?:int|void)\s*$',clean[line_start:start]): continue
        open_paren=clean.find('(',start); depth=0; close=None
        for j in range(open_paren,len(clean)):
            if clean[j]=='(': depth+=1
            elif clean[j]==')':
                depth-=1
                if depth==0: close=j; break
        if close is None: violations.append({'path':rel,'issue':'unclosed call'}); continue
        k=close+1
        while k<len(clean) and clean[k].isspace(): k+=1
        if k>=len(clean) or clean[k]!=';':
            violations.append({'path':rel,'issue':'call result is part of a larger expression'}); continue
        stmt_start=max(clean.rfind(';',0,start),clean.rfind('{',0,start),clean.rfind('}',0,start))+1
        prefix=clean[stmt_start:start].strip()
        if prefix.startswith('::'): prefix=prefix[2:].strip()
        if prefix and not re.fullmatch(r'if\s*\(.*\)',prefix,re.S):
            violations.append({'path':rel,'issue':'non-statement prefix: '+prefix[-80:]})
        arg_text=clean[open_paren+1:close]; args=split_args(arg_text)
        if not args or args[-1]!='false': bool_values[rel]=args[-1] if args else None
        calls+=1
    total+=calls
    files.append({'path':rel,'void_prototypes':void_declarations,'int_prototypes_remaining':int_declarations,'call_expressions':calls})
    if void_declarations!=1 or int_declarations!=0:
        violations.append({'path':rel,'issue':'prototype count mismatch','void':void_declarations,'int':int_declarations})
result={'caller_files':len(files),'total_calls':total,'all_results_unused':not violations,'non_false_final_arguments':bool_values,'violations':violations,'files':files}
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\debug-providers-luna\proof\maybe_draw_text_use_audit.json')
out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='files'},indent=2))
print('audit='+str(out))
