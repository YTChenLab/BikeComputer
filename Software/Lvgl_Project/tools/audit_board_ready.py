"""Audit a configured STM32 source manifest and ARM ELF object sizes, without extra packages."""
from pathlib import Path
import argparse, csv, hashlib, json, re, struct

def elf_symbols(path):
    data=path.read_bytes()
    if data[:6]!=b'\x7fELF\x01\x01': return []
    if struct.unpack_from('<H',data,18)[0]!=40: raise ValueError(f'非 ARM ELF：{path}')
    offset=struct.unpack_from('<I',data,32)[0]
    size,count,names=struct.unpack_from('<HHH',data,46)
    sections=[struct.unpack_from('<IIIIIIIIII',data,offset+i*size) for i in range(count)]
    def content(s):return data[s[4]:s[4]+s[5]]
    def string(d,i):return d[i:d.index(b'\0',i)].decode('utf-8',errors='replace')
    section_names=content(sections[names]);out=[]
    for s in sections:
        if s[1]!=2:continue
        strings=content(sections[s[6]])
        for off in range(s[4],s[4]+s[5],s[9]):
            name,value,length,info,other,section=struct.unpack_from('<IIIBBH',data,off)
            if not name or section>=len(sections):continue
            sec=sections[section]
            out.append({'name':string(strings,name),'bytes':length,'kind':info&15,
                'section':string(section_names,sec[0]) if section else '',
                'writable':bool(sec[2]&1),'undefined':section==0})
    return out

def main():
    parser=argparse.ArgumentParser(description='STM32 工程源文件、依赖与 ARM 大对象审计')
    parser.add_argument('--build',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--probe',type=Path)
    args=parser.parse_args();root=Path(__file__).resolve().parent.parent
    build=args.build.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    commands=json.loads((build/'compile_commands.json').read_text(encoding='utf-8'))
    sources=sorted({Path(c['file']).resolve().relative_to(root).as_posix() for c in commands})
    forbidden=re.compile(r'^src/(main\.c|hal/|platform/pc/|perf_benchmark\.c)|^FreeRTOS/|^tests/|^lvgl/(demos|examples)/|/src/drivers/(sdl|windows|glfw|x11|wayland|evdev|libinput|qnx|nuttx|display/(drm|fb))/|/src/libs/fsdrv/lv_fs_(posix|stdio|win32)\.c$')
    violations=[s for s in sources if forbidden.search(s)]
    common=[p for d in ['src/ui','src/services','src/platform/stm32'] for p in (root/d).rglob('*') if p.suffix in ['.c','.h']]
    atoms=[];pc=[];lvgl=[];blocking=[];service_lvgl=[]
    dependency=re.compile(r'\b(fopen|fread|fwrite|localtime_s|localtime_r|WinHttp\w*|pthread_\w*|SDL_\w*|Sleep)\s*\(|#\s*include\s*[<"](windows\.h|SDL[^>"]*|pthread\.h)')
    for p in common:
        for n,line in enumerate(p.read_text(encoding='utf-8-sig').splitlines(),1):
            item={'source':p.relative_to(root).as_posix(),'line':n,'text':line.strip()}
            if re.search(r'atomic_|stdatomic|_Static_assert',line):atoms.append(item)
            if dependency.search(line):pc.append(item)
            if p.parent.name=='stm32' and re.search(r'\blv_\w+\s*\(',line):lvgl.append(item)
            if re.search(r'(->read_at\s*\(|s_port\.(load|store)\s*\(|storage->(load|store|append_record|load_page|store_checkpoint)\s*\()',line):blocking.append(item)
            if 'services' in p.parts and re.search(r'\blv_\w+\s*\(',line):service_lvgl.append(item)
    memory=[];sdram=[];undefined=set()
    for p in build.rglob('*.o'):
        if 'CMakeScratch' in p.parts:continue
        for r in elf_symbols(p):
            if r['undefined']:undefined.add(r['name'])
            if r['kind']==1 and r['bytes']>=1024:
                r['object']=p.relative_to(build).as_posix();memory.append(r)
            if r['kind']==1 and r['section']=='.ui_sdram':
                r['object']=p.relative_to(build).as_posix();sdram.append(r)
    memory.sort(key=lambda r:(r['object'],r['name']))
    misplaced=[r for r in memory if r['writable'] and r['section']!='.ui_sdram']
    probe=[]
    if args.probe:
        probe=[r for r in elf_symbols(args.probe) if r['name'].startswith('board_size_')]
    report={'sources':sources,'source_violations':violations,'common_pc_dependencies':pc,
        'c11_atomic_sites':atoms,'stm32_lvgl_calls':lvgl,'large_objects':memory,
        'potential_blocking_sites':blocking,'service_lvgl_sites':service_lvgl,
        'sdram_objects':sdram,'sdram_static_bytes':sum(r['bytes'] for r in sdram),
        'misplaced_large_ram':misplaced,'abi_sizes':{r['name'][11:]:r['bytes'] for r in probe},
        'large_sdram_bytes':sum(r['bytes'] for r in memory if r['section']=='.ui_sdram'),
        'undefined_symbols':sorted(undefined),'file_sha256':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in common}}
    (out/'审计数据.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    with (out/'ARM大对象符号.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['object','name','bytes','section','writable','kind','undefined']);writer.writeheader();writer.writerows(memory)
    print(json.dumps({'编译源文件':len(sources),'平台违规':len(violations),'公共PC依赖':len(pc),
        '大对象':len(memory),'大RAM未重定位':len(misplaced),'大对象SDRAM字节':report['large_sdram_bytes']},ensure_ascii=False))
    if violations or pc or misplaced:raise SystemExit(1)
if __name__=='__main__':main()
