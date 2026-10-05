"""Reproducible Xi'an download + conversion. No third-party Python packages.

Usage: python tools/download_xian_map.py --source-dir D:/map-source
Use --reuse only for a previously completed, validated download.
Never downloads rendered web tiles. Atomic output protects previous packages.
"""
import argparse, hashlib, json, os, subprocess, sys, urllib.request
from pathlib import Path
QUERY='[out:json][timeout:180];(way[highway](34.12,108.78,34.46,109.15);way[waterway](34.12,108.78,34.46,109.15);way[natural~"water|wood"](34.12,108.78,34.46,109.15);way[leisure~"park|garden"](34.12,108.78,34.46,109.15);way[landuse~"forest|grass"](34.12,108.78,34.46,109.15);way[building](34.25,108.92,34.28,108.98);relation[natural=water](34.12,108.78,34.46,109.15);relation[leisure=park](34.12,108.78,34.46,109.15););out body;>;out skel qt;'
URL='https://overpass.private.coffee/api/interpreter'
def main():
    p=argparse.ArgumentParser();p.add_argument('--source-dir',type=Path,required=True);p.add_argument('--reuse',action='store_true');p.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'assets/maps/xian')
    a=p.parse_args();a.source_dir.mkdir(parents=True,exist_ok=True);source=a.source_dir/'xian.osm.json'
    if not a.reuse:
        temporary=source.with_suffix('.json.part');req=urllib.request.Request(URL,data=QUERY.encode(),headers={'Content-Type':'text/plain','User-Agent':'BikeComputer-OfflineMap/1.0'})
        with urllib.request.urlopen(req,timeout=240) as response,temporary.open('wb') as f:
            n=0
            while True:
                chunk=response.read(1024*1024)
                if not chunk:break
                f.write(chunk);n+=len(chunk);print(f'已下载 {n/1048576:.1f} MiB',flush=True)
            f.flush();os.fsync(f.fileno())
        document=json.loads(temporary.read_bytes())
        if document.get('remark') or not document.get('elements'):raise RuntimeError('地图接口返回错误或空数据，原有文件未替换')
        temporary.replace(source)
    (a.source_dir/'overpass-query.txt').write_text(QUERY,encoding='ascii')
    subprocess.run([sys.executable,str(Path(__file__).with_name('build_offline_map.py')),str(source),str(a.output)],check=True)
    project=Path(__file__).resolve().parents[1];extra=project/'tools/ui_extra_characters.txt'
    current=extra.read_text(encoding='utf-8') if extra.exists() else ''
    additions=(a.output/'map_characters.txt').read_text(encoding='utf-8')
    extra.write_text(current+'\n'+additions if additions.strip() not in current else current,encoding='utf-8')
    print('地图包已生成，重新构建工程即可更新地图地名字体。')
if __name__=='__main__':main()
