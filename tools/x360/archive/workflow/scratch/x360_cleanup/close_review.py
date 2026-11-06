from pathlib import Path
import json
import re
import subprocess
import strip_host as s

removed = []
for name in s.source_files():
    path=s.ROOT/name;data=path.read_bytes();spans=[]
    nodes=list(s.walk(s.PARSER.parse(data).root_node))
    profile_locals=set()
    for node in nodes:
        if node.type=='declaration' and re.match(r'(?:Stage|Scope)\s+l\w+\(',s.text_of(node,data).strip()):
            profile_locals.update(s.declarator_names(node,data))
            spans.append(s.erase_span(node,data))
        if node.type=='enum_specifier' and 'EPCWaitResult' in s.declarator_names(node,data):spans.append(s.erase_span(node,data,True))
        if node.type=='function_definition':
            code=data[node.start_byte:node.end_byte]
            if re.search(rb'\bE_PCWAIT_\w+|\b__ImageBase\b',code):
                spans.append(s.erase_span(node,data,True));removed.append((name,s.declarator_names(node,data)))
        if node.type=='qualified_identifier' and re.match(r'renderengine::(?:Device(?:::|$)|gDevice$)',s.text_of(node,data)):
            current=node
            while current and current.type not in s.STATEMENTS|s.DECLARATIONS:current=current.parent
            if current:
                if current.type=='return_statement':current=s.containing(current,'function_definition')
                if current and current.parent and current.parent.type=='condition_clause':current=current.parent.parent
                if current:spans.append(s.erase_span(current,data))
    for node in nodes:
        if node.type in s.IDENTIFIERS and s.text_of(node,data) in profile_locals:
            current=node
            while current and current.type not in s.STATEMENTS|s.DECLARATIONS:current=current.parent
            if current:spans.append(s.erase_span(current,data))
    if spans:path.write_bytes(s.apply_spans(data,spans))

# Prune only newly deleted translation units from the existing build lists.
deleted=set(subprocess.check_output(['git','diff','--name-only','--diff-filter=D'],cwd=s.ROOT,text=True).splitlines())
for name in s.FILES:
    path=s.ROOT/name
    if path.name!='CMakeLists.txt' or not path.exists() or '/pc/' in name.lower():continue
    text=path.read_text();kept=[]
    for line in text.splitlines(keepends=True):
        m=re.match(r'\s*"?([^"\n]+\.(?:cpp|h|hpp|c|inl))"?\s*$',line)
        if m:
            value=m.group(1).strip().replace('${CMAKE_CURRENT_SOURCE_DIR}/','')
            if (Path(name).parent/value).as_posix() in deleted:continue
        kept.append(line)
    result=''.join(kept)
    result=re.sub(r'target_sources\([^\s()]+\s+(?:INTERFACE|PRIVATE|PUBLIC)\s*\)\s*','',result)
    if result!=text:path.write_bytes(result.replace('\n','\r\n').encode('utf-8'))

readme=s.ROOT/'README.md'
text=readme.read_text().replace('Missing implementations are left unresolved.', 'Missing implementations are left unresolved. Mixed methods that depend on PC\ninfrastructure are removed as whole definitions when their console implementation\nis still missing; the base commit is `209fb3b94f7266506dff4ffd569e8553f1b3af93`.')
readme.write_bytes(text.replace('\n','\r\n').encode('utf-8'))

(s.OUT/'native_gaps.json').write_text(json.dumps(removed,indent=2),encoding='utf-8')
print('Removed host-dependent wait and image-base implementations:',removed)
