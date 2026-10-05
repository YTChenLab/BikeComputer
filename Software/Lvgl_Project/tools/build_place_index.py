"""Build a bounded offline place index from retained OSM and official Unihan."""
import argparse,collections,hashlib,itertools,json,re,struct,unicodedata,zipfile,zlib
from pathlib import Path
from map_places import collect,BBOX,district_regions

def fixed(text,n):
    data=text.encode('utf8')[:n-1]
    while data:
        try:data.decode('utf8');break
        except UnicodeDecodeError:data=data[:-1]
    return data.ljust(n,b'\0')

def ascii_key(text):
    text=text.lower().replace('ü','v')
    return re.sub('[^a-z0-9]','',unicodedata.normalize('NFKD',text).encode('ascii','ignore').decode())

def display_name(text):
    if len(text.encode('utf8'))<=63:return text
    qualifier=re.search(r'[（(][^（）()]+[）)]$',text)
    if qualifier and len(qualifier[0].encode('utf8'))<=39:
        suffix='…'+qualifier[0];budget=63-len(suffix.encode('utf8'))
        prefix=fixed(text[:qualifier.start()],budget+1).split(b'\0',1)[0].decode('utf8')
        return prefix+suffix
    return fixed(text,64).split(b'\0',1)[0].decode('utf8')

def main():
    p=argparse.ArgumentParser();p.add_argument('output',type=Path);p.add_argument('--source',type=Path,action='append',required=True);p.add_argument('--unihan',type=Path,required=True)
    p.add_argument('--districts',type=Path);p.add_argument('--bbox',type=float,nargs=4,default=BBOX)
    p.add_argument('--graph',type=Path,help='Keep only entrances within routing snap range')
    a=p.parse_args();bbox=a.bbox
    readings={}
    with zipfile.ZipFile(a.unihan) as z:
        for line in z.read('Unihan_Readings.txt').decode().splitlines():
            if '\tkMandarin\t' in line:
                code,_,value=line.split('\t');readings[chr(int(code[2:],16))]=[ascii_key(v) for v in value.split()][:2]
    # Bounded common alternatives, no general-purpose Chinese IME dependency.
    for c,values in {'长':['chang','zhang'],'重':['zhong','chong'],'乐':['le','yue'],'单':['dan','shan'],'厦':['xia','sha'],'行':['xing','hang']}.items():readings[c]=values
    documents=[json.loads(s.read_bytes()) for s in a.source];elements={}
    if any(d.get('remark') or not isinstance(d.get('elements'),list) for d in documents):raise ValueError('Incomplete OSM source')
    for d in sorted(documents,key=lambda d:d.get('osm3s',{}).get('timestamp_osm_base','')):
        for e in d['elements']:
            key=(e['type'],e['id']);old=elements.get(key,{});merged={**old,**e,'tags':e.get('tags',{})}
            if e['type']=='way' and 'nodes' in e:
                for field in ('center','geometry'):
                    if field not in e:merged.pop(field,None)
            elements[key]=merged
    regions=district_regions(json.loads(a.districts.read_bytes())) if a.districts else None
    places=collect(elements,roads=True,bbox=bbox,regions=regions);records=bytearray();keys=set();ids=set();unsupported=set()
    graph_cells={}
    if a.graph:
        graph=a.graph.read_bytes()
        if graph[:8]!=b'BGRF0001':raise ValueError('Invalid routing graph')
        for lon,lat,first,count in struct.iter_unpack('<iiII',graph[128:128+struct.unpack_from('<I',graph,16)[0]*16]):
            if count:graph_cells.setdefault((lon//10000,lat//10000),[]).append((lon/1e7,lat/1e7))
    for i,place in enumerate(places):
        identity=zlib.crc32((place['osm_type']+str(place['osm_id'])).encode()) or 1
        while identity in ids:identity=(identity+1)&0xffffffff or 1
        route=place['route']
        if route and a.graph:
            x,y=int(route[0]*1e7)//10000,int(route[1]*1e7)//10000
            near=[v for dx in range(-2,3) for dy in range(-2,3) for v in graph_cells.get((x+dx,y+dy),[])]
            if not any(((v[0]-route[0])*92000)**2+((v[1]-route[1])*111319)**2<=160**2 for v in near):route=None;place['entrance']=False
        place['route']=route
        extra=struct.pack('<iiII',round(route[0]*1e7),round(route[1]*1e7),1,0) if route else b'\0'*16
        ids.add(identity);records+=struct.pack('<IiiBBH',identity,round(place['lon']*1e7),round(place['lat']*1e7),place['kind'],int(place['entrance']),0)+fixed(display_name(place['name']),64)+fixed(place['detail'],64)+extra
        for name in [place['name']]+place['aliases']:
            literal=re.sub(r'[^a-z0-9\u3400-\u9fff]','',unicodedata.normalize('NFKC',name).lower())
            literal=fixed(literal,64).split(b'\0',1)[0].decode('utf8')
            if len(literal)>=2:keys.add((literal,i))
            options=[]
            for c in name:
                if c in readings:options.append(readings[c])
                elif c.isascii() and c.isalnum():options.append([c.lower()])
                elif '\u3400'<=c<='\u9fff':unsupported.add(c);options.append([''])
            for variant in itertools.islice(itertools.product(*options),8):
                for start in range(len(variant)):
                    full=''.join(variant[start:])[:63];initial=''.join(v[:1] for v in variant[start:])[:63]
                    for key in (full,initial):
                        if len(key)>=2:keys.add((key,i))
    indexes=b''.join(fixed(k,64)+struct.pack('<I',i) for k,i in sorted(keys));payload=records+indexes
    header=bytearray(struct.pack('<8s8I',b'BPOI0001',1,2,len(places),len(keys),128,128+len(records),128+len(payload),zlib.crc32(payload)).ljust(128,b'\0'))
    struct.pack_into('<I4i',header,44,0,*[round(x*1e7) for x in bbox]);struct.pack_into('<I',header,40,zlib.crc32(header[:40]+header[44:]))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(header+payload)
    manifest={'format':'BPOI0001','revision':2,'coordinate_system':'WGS84','bbox_lon_lat':bbox,'district_counts':dict(collections.Counter(v['district'] for v in places)),'mapped_entrances':sum(bool(v['route']) for v in places),'places':len(places),'kinds':dict(collections.Counter(str(v['kind']) for v in places)),'keys':len(keys),'bytes':a.output.stat().st_size,'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'sources':[{'file':s.name,'sha256':hashlib.sha256(s.read_bytes()).hexdigest(),'timestamp':d.get('osm3s',{}).get('timestamp_osm_base')} for s,d in zip(a.source,documents)],'pronunciation_source':{'url':'https://www.unicode.org/Public/17.0.0/ucd/Unihan.zip','sha256':hashlib.sha256(a.unihan.read_bytes()).hexdigest(),'version':'17.0.0'},'unsupported_reading_characters':sorted(unsupported),'limitations':['OSM named places and mapped addresses; completeness is not guaranteed.','Mapped entrances need field confirmation; absent entrances use a display anchor.','Common polyphonic alternatives limited to eight variants per name.'],'license':'OSM ODbL-1.0; Unihan Unicode-3.0'}
    a.output.with_suffix('.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    a.output.with_suffix('.places.json').write_text(json.dumps(places,ensure_ascii=False,indent=2),encoding='utf8')
    extra=Path(__file__).resolve().parent/'ui_extra_characters.txt';extra.write_text(''.join(sorted(set(extra.read_text(encoding='utf8')+'…'+''.join(p['name']+p['detail'] for p in places))))+'\n',encoding='utf8')
    print(json.dumps({k:manifest[k] for k in ('places','keys','bytes','kinds')},ensure_ascii=False))
if __name__=='__main__':main()
