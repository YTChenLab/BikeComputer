"""Build matched vector/road/place resources from validated district tile caches.

The existing central colour tiles stay usable. The PC provider falls back to
the expanded vector map outside their coverage. No rendered tiles downloaded.
"""
import argparse, hashlib, json, math, shutil, subprocess, sys, time, zipfile
from pathlib import Path
from map_places import DistrictLookup, district_regions

def main():
    p=argparse.ArgumentParser();p.add_argument('cache',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--districts',type=Path,required=True);p.add_argument('--source',type=Path,action='append',required=True)
    p.add_argument('--unihan',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    a=p.parse_args();coverage=json.loads((a.cache/'coverage.json').read_bytes())
    if coverage.get('complete') is not True:raise ValueError('Partial coverage must not replace production resources')
    sources=a.source+[a.cache/t['file'] for t in coverage['tiles']]
    for t in coverage['tiles']:
        s=a.cache/t['file'];meta=json.loads(s.with_suffix('.source.json').read_bytes())
        if hashlib.sha256(s.read_bytes()).hexdigest()!=meta['sha256']:raise ValueError('Cached source hash mismatch: '+s.name)
    docs=[json.loads(s.read_bytes()) for s in sources]
    if any(d.get('remark') or not isinstance(d.get('elements'),list) for d in docs):raise ValueError('Incomplete OSM response')
    regions=district_regions(json.loads(a.districts.read_bytes()));lookup=DistrictLookup(regions)
    elements={}
    for d in sorted(docs,key=lambda d:d.get('osm3s',{}).get('timestamp_osm_base','')):
        for e in d['elements']:
            k=e['type'],e['id'];old=elements.get(k,{})
            merged={**old,**e,'tags':e.get('tags',{})}
            if e['type']=='way' and 'nodes' in e:
                for field in ('center','geometry'):
                    if field not in e:merged.pop(field,None)
            elements[k]=merged
    # Overpass geom output has node references without separate node records.
    for e in list(elements.values()):
        if e['type']=='way' and e.get('geometry'):
            for n,v in zip(e.get('nodes',[]),e['geometry']):
                if v:elements.setdefault(('node',n),{'type':'node','id':n,**v})
    nodes={e['id']:(e['lon'],e['lat']) for e in elements.values() if e['type']=='node' and 'lon' in e and lookup((e['lon'],e['lat']))}
    ways={e['id']:e for e in elements.values() if e['type']=='way'}
    a.work.mkdir(parents=True,exist_ok=True);a.output.mkdir(parents=True,exist_ok=True)
    import build_color_map as color
    color.BBOX=tuple(coverage['bbox']);color.MX=111319.49*math.cos(math.radians((color.BBOX[1]+color.BBOX[3])/2))
    graph=color.build_graph(ways,nodes,a.work,int(time.time()))
    if graph['nodes']>500000:raise ValueError('Graph exceeds firmware reader node bound')
    if not graph['nodes'] or not graph['directed_edges']:raise ValueError('Empty routing graph')
    print('graph',graph,flush=True)
    merged=a.work/'merged-navigation.json'
    merged.write_text(json.dumps({'osm3s':{'timestamp_osm_base':max(d.get('osm3s',{}).get('timestamp_osm_base','') for d in docs)},
        'elements':list(elements.values())},ensure_ascii=False),encoding='utf8')
    tools=Path(__file__).resolve().parent
    subprocess.run([sys.executable,str(tools/'build_offline_map.py'),str(merged),str(a.work),'--bbox',*[str(x) for x in color.BBOX]],check=True)
    vector_chars=(a.work/'map_characters.txt').read_text(encoding='utf8')
    font_seed=tools/'ui_extra_characters.txt'
    font_seed.write_text(''.join(sorted(set(font_seed.read_text(encoding='utf8')+vector_chars)))+'\n',encoding='utf8')
    command=[sys.executable,str(tools/'build_place_index.py'),str(a.work/'xian-places.bpoi'),
        '--source',str(merged),'--unihan',str(a.unihan),'--districts',str(a.districts),
        '--graph',str(a.work/'xian-cycle.bgraph'),'--bbox',*[str(x) for x in color.BBOX]]
    subprocess.run(command,check=True)
    # Publish into the staging project only after every builder succeeds.
    for name in ('xian-urban.bmap','xian-cycle.bgraph','xian-places.bpoi','xian-places.json','xian-places.places.json'):
        shutil.copy2(a.work/name,a.output/name)
    shutil.copy2(a.work/'manifest.json',a.output/'vector-manifest.json')
    map_chars=a.output/'map_characters.txt'
    map_chars.write_text(''.join(sorted(set(map_chars.read_text(encoding='utf8')+vector_chars)))+'\n',encoding='utf8')
    manifest={'coverage':coverage,'graph':graph,'colour_bbox':[108.78,34.12,109.15,34.46],
        'coordinate_system':'WGS84','licence':'ODbL-1.0','attribution':'© OpenStreetMap contributors',
        'sources':[{'file':s.name,'sha256':hashlib.sha256(s.read_bytes()).hexdigest()} for s in sources+[a.districts]],
        'limits':['OSM coverage and access may differ from reality.','Colour coverage is smaller than vector/road/search coverage.',
                  'High-tech coverage follows Yanta and Changan district boundaries; other managed districts are not included.']}
    (a.output/'navigation-coverage.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    # The replay route and central colour tiles have independent snapshots.
    original=json.loads((a.output/'manifest.json').read_bytes())
    vector=json.loads((a.work/'manifest.json').read_bytes())
    vector['source']='https://download.geofabrik.de/asia/china/shaanxi.html'
    vector['route']=original['route'];vector['route_source_note']='Original central replay retained; see xian-city.broute checksum.'
    vector['search_coverage_districts']=coverage['districts']
    (a.output/'manifest.json').write_text(json.dumps(vector,ensure_ascii=False,indent=2),encoding='utf8')
    colour=json.loads((a.output/'color_manifest.json').read_bytes())
    if 'original_graph' not in colour:colour['original_graph']=colour['graph'];colour['original_graph_sha256']=colour['graph_sha256']
    colour['graph']=graph;colour['graph_sha256']=hashlib.sha256((a.output/'xian-cycle.bgraph').read_bytes()).hexdigest()
    colour['graph_source']='navigation-coverage.json';colour['graph_bbox_lon_lat']=coverage['bbox']
    (a.output/'color_manifest.json').write_text(json.dumps(colour,ensure_ascii=False,indent=2),encoding='utf8')
    with zipfile.ZipFile(a.output/'navigation-sources-20261005.zip','w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for s in sources+[a.districts,a.cache/'coverage.json']:
            z.writestr(s.name,s.read_bytes())
            meta=s.with_suffix('.source.json')
            if meta.exists():z.writestr(meta.name,meta.read_bytes())
    (a.output/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest()+'  '+f.name+'\n'
        for f in sorted(a.output.iterdir()) if f.suffix in ('.btiles','.bmap','.bgraph','.bpoi','.broute','.gpx','.zip')),encoding='ascii',newline='\n')
    print('Published staged resources',a.output,flush=True)

if __name__=='__main__':main()
