from pathlib import Path
import json
import re
import subprocess
import strip_host as s

identities=set(json.loads((s.ROOT.parent/'progress/identity.json').read_text()))
proc=subprocess.Popen(['git','cat-file','--batch'],cwd=s.ROOT,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
restored=[]
for name in s.source_files():
    path=s.ROOT/name;now=path.read_bytes()
    proc.stdin.write(('HEAD:'+name+'\n').encode());proc.stdin.flush()
    header=proc.stdout.readline().decode().split();old=proc.stdout.read(int(header[-1]));proc.stdout.read(1)
    if b'renderengine::Device::' not in old:continue
    current_fns={tuple(s.declarator_names(n,now)):n for n in s.walk(s.PARSER.parse(now).root_node) if n.type=='function_definition'}
    targets=[]
    for node in s.walk(s.PARSER.parse(old).root_node):
        if node.type!='call_expression':continue
        function=node.child_by_field_name('function')
        if not function:continue
        symbol=s.text_of(function,old)
        if not symbol.startswith('renderengine::Device::') or symbol not in identities:continue
        owner=s.containing(node,'function_definition')
        if not owner:continue
        key=tuple(s.declarator_names(owner,old))
        if key not in current_fns:continue
        statement=node
        while statement and statement.type not in {'expression_statement','declaration','return_statement'}:statement=statement.parent
        if not statement:continue
        code=s.text_of(statement,old)
        if re.sub(r'\s+','',code) in re.sub(r'\s+','',s.text_of(current_fns[key],now)):continue
        targets.append((key,symbol,statement))
    # Only the final native presentation remains; earlier presentations belong
    # to removed PC fallback branches.
    show=[t[2].start_byte for t in targets if t[1]=='renderengine::Device::ShowPixelBuffer']
    insertions=[]
    for key,symbol,statement in targets:
        if symbol=='renderengine::Device::ShowPixelBuffer' and statement.start_byte!=max(show):continue
        current=current_fns[key]
        anchor=statement.prev_named_sibling
        location=None
        while anchor:
            if anchor.type!='comment':
                code=re.sub(r'\s+','',s.text_of(anchor,old))
                matches=[n for n in s.walk(current) if n.type==anchor.type and re.sub(r'\s+','',s.text_of(n,now))==code]
                if len(matches)==1:
                    location=matches[0].end_byte;break
            anchor=anchor.prev_named_sibling
        if location is None:
            body=current.child_by_field_name('body');location=body.start_byte+1
        text=s.text_of(statement,old)
        indent=' '*statement.start_point.column
        insertions.append((location,('\r\n'+indent+text+'\r\n').encode('utf-8')))
        restored.append((name,key,symbol))
    for location,text in sorted(insertions,reverse=True):now=now[:location]+text+now[location:]
    if insertions:path.write_bytes(now)
proc.stdin.close();proc.wait()
print('Restored attested native Device operations:',restored)
