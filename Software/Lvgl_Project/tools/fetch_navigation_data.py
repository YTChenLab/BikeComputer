"""Cache complete OSM district snapshots; fail rather than publish partial coverage.

Sequential requests only. Resume by rerunning with the same output directory.
No API keys, new packages, rendered provider tiles, or silent error suppression.
"""
import argparse, gzip, hashlib, json, math, time, urllib.parse, urllib.request
from pathlib import Path

DISTRICTS = {'新城区':3226096,'碑林区':3226088,'莲湖区':3226093,
             '雁塔区':3226098,'灞桥区':3226087,'未央区':3226095,'长安区':3226089}
ENDPOINTS = ('https://maps.mail.ru/osm/tools/overpass/api/interpreter',
             'https://overpass-api.de/api/interpreter',
             'https://overpass.private.coffee/api/interpreter')

def fetch(output, name, query):
    path = output / (name + '.json')
    if path.exists():
        doc = json.loads(path.read_bytes())
        if doc.get('remark') or not isinstance(doc.get('elements'),list):
            raise ValueError('Invalid cached response: ' + name)
        return doc
    for endpoint in ENDPOINTS:
        try:
            request = urllib.request.Request(endpoint,data=urllib.parse.urlencode({'data':query}).encode(),
                headers={'User-Agent':'BikeComputer-offline-preparation/1.1','Accept-Encoding':'gzip',
                         'Content-Type':'application/x-www-form-urlencoded'})
            with urllib.request.urlopen(request, timeout=65) as response:
                raw = response.read()
                if response.headers.get('Content-Encoding')=='gzip':raw=gzip.decompress(raw)
            doc = json.loads(raw)
            if doc.get('remark') or not isinstance(doc.get('elements'),list):
                raise ValueError('Incomplete or empty Overpass response')
            temporary = path.with_suffix('.tmp')
            temporary.write_bytes(raw); temporary.replace(path)
            path.with_suffix('.source.json').write_text(json.dumps({
                'query':query,'endpoint':endpoint,'sha256':hashlib.sha256(raw).hexdigest(),
                'snapshot':doc.get('osm3s',{}).get('timestamp_osm_base'),
                'license':'ODbL-1.0','coordinate_system':'WGS84'}, ensure_ascii=False, indent=2), encoding='utf8')
            print(name, len(doc['elements']), len(raw), flush=True)
            time.sleep(2)
            return doc
        except Exception as error:
            print(name, type(error).__name__, str(error)[:120], flush=True)
            if getattr(error,'code',0) in (429,406): time.sleep(30)
    raise RuntimeError('Cannot validate complete source: ' + name)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path)
    parser.add_argument('--kind',choices=('places','roads','all'),default='all')
    parser.add_argument('--districts',type=Path,help='Use small bounding-box tiles for busy public servers')
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    if args.districts:
        doc=json.loads(args.districts.read_bytes())
        bounds=[]
        for e in doc['elements']:
            b=e['bounds'];bounds.append((b['minlon'],b['minlat'],b['maxlon'],b['maxlat']))
        west=math.floor(min(b[0] for b in bounds)*100)/100;south=math.floor(min(b[1] for b in bounds)*100)/100
        east=math.ceil(max(b[2] for b in bounds)*100)/100;north=math.ceil(max(b[3] for b in bounds)*100)/100
        tiles=[];step=.16
        for y in reversed(range(math.ceil((north-south)/step))):
            for x in range(math.ceil((east-west)/step)):
                b=(west+x*step,south+y*step,min(east,west+(x+1)*step),min(north,south+(y+1)*step))
                if not any(b[0]<v[2] and b[2]>v[0] and b[1]<v[3] and b[3]>v[1] for v in bounds):continue
                area=f'({b[1]:.6f},{b[0]:.6f},{b[3]:.6f},{b[2]:.6f})'
                tags=('["amenity"]["name"]','["shop"]["name"]','["tourism"]["name"]',
                      '["leisure"]["name"]','["historic"]["name"]','["railway"="station"]["name"]',
                      '["landuse"~"^(residential|education)$"]["name"]','["addr:housenumber"]["addr:street"]')
                name=f'tile-{y:02}-{x:02}'
                if (args.output/(name+'.json')).exists():tiles.append({'file':name+'.json','bbox':b});continue
                query='[out:json][timeout:25];way["highway"]'+area+';(._;>;);out body;'
                fetch(args.output,name+'-roads',query);tiles.append({'file':name+'-roads.json','bbox':b})
                query='[out:json][timeout:25];('+''.join('nwr'+tag+area+';' for tag in tags)
                query+='node["entrance"]'+area+';node["barrier"~"^(gate|lift_gate|swing_gate)$"]'+area+';);out body center;'
                fetch(args.output,name+'-places',query);tiles.append({'file':name+'-places.json','bbox':b})
        (args.output/'coverage.json').write_text(json.dumps({'bbox':(west,south,east,north),
            'districts':list(DISTRICTS),'tiles':tiles,'complete':True},ensure_ascii=False,indent=2),encoding='utf8')
        print('Validated complete coverage',len(tiles),(west,south,east,north),flush=True)
        return
    for name,relation in DISTRICTS.items():
        prefix=f'[out:json][timeout:45];area({3600000000+relation})->.scope;'
        if args.kind in ('places','all'):
            query=prefix+'('+''.join('nwr'+tag+'(area.scope);' for tag in (
                '["amenity"]["name"]','["shop"]["name"]','["tourism"]["name"]',
                '["historic"]["name"]','["leisure"~"^(park|garden|sports_centre|stadium)$"]["name"]',
                '["railway"~"^(station|halt)$"]["name"]','["public_transport"="station"]["name"]',
                '["landuse"~"^(residential|education)$"]["name"]',
                '["building"~"^(apartments|commercial|retail)$"]["name"]',
                '["addr:housenumber"]'))+'node["entrance"](area.scope);node["barrier"~"^(gate|lift_gate|swing_gate)$"](area.scope););out body geom;'
            fetch(args.output,name+'-places',query)
        if args.kind in ('roads','all'):
            fetch(args.output,name+'-roads',prefix+'way["highway"](area.scope);(._;>;);out body;')
    paths=sorted(args.output.glob('*-'+('places' if args.kind=='places' else 'roads')+'.json'))
    print('Validated district files',len(paths),flush=True)

if __name__=='__main__':main()
