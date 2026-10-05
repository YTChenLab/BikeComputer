"""Read current raw/zlib OSM PBF extracts using only Python's standard library.

Supports simple and dense nodes, signed deltas, offsets, ways and relations.
Rejects history, unknown required features, truncated or oversized blocks.
Format: https://wiki.openstreetmap.org/wiki/PBF_Format and OSM-binary proto.
Two passes make node/way filtering independent of entity ordering.
"""
import argparse,datetime,hashlib,json,struct,zlib
from pathlib import Path
from map_places import category

def varint(data,at):
    value=0
    for shift in range(0,70,7):
        if at>=len(data):raise ValueError('Truncated protobuf varint')
        c=data[at];at+=1;value|=(c&127)<<shift
        if not c&128:return value,at
    raise ValueError('Oversized protobuf varint')

def message(data):
    result={};at=0
    while at<len(data):
        tag,at=varint(data,at);number,wire=tag>>3,tag&7
        if not number:raise ValueError('Invalid protobuf field')
        if wire==0:value,at=varint(data,at)
        elif wire==2:
            length,at=varint(data,at)
            if length>len(data)-at:raise ValueError('Truncated protobuf field')
            value=data[at:at+length];at+=length
        elif wire in (1,5):
            length=8 if wire==1 else 4
            if length>len(data)-at:raise ValueError('Truncated fixed field')
            value=data[at:at+length];at+=length
        else:raise ValueError('Unsupported protobuf wire type')
        result.setdefault(number,[]).append(value)
    return result

def scalar(m,k,default=0):return m.get(k,[default])[0]
def signed(v):return (v>>1)^-(v&1)
def int64(v):return v-(1<<64) if v&(1<<63) else v
def packed(m,k):
    out=[]
    for value in m.get(k,[]):
        if isinstance(value,int):out.append(value);continue
        at=0
        while at<len(value):v,at=varint(value,at);out.append(v)
    return out
def deltas(values):
    total=0;out=[]
    for v in values:total+=signed(v);out.append(total)
    return out

def blocks(path):
    header_seen=False
    with path.open('rb') as f:
        while True:
            prefix=f.read(4)
            if not prefix:break
            if len(prefix)!=4:raise ValueError('Truncated PBF length')
            length=struct.unpack('>I',prefix)[0]
            if not 0<length<65536:raise ValueError('Invalid PBF header length')
            raw=f.read(length)
            if len(raw)!=length:raise ValueError('Truncated PBF header')
            h=message(raw);size=scalar(h,3);kind=scalar(h,1,b'')
            if not 0<size<=32*1024*1024:raise ValueError('Invalid PBF blob size')
            raw=f.read(size)
            if len(raw)!=size:raise ValueError('Truncated PBF blob')
            b=message(raw)
            if 1 in b:payload=scalar(b,1)
            elif 3 in b:
                expected=scalar(b,2)
                if not 0<expected<=32*1024*1024:raise ValueError('Invalid raw block size')
                decoder=zlib.decompressobj();payload=decoder.decompress(scalar(b,3),expected+1)
                if len(payload)!=expected or not decoder.eof or decoder.unused_data:raise ValueError('Invalid compressed block')
            else:raise ValueError('Only raw/zlib PBF blobs supported')
            if len(payload)>32*1024*1024:raise ValueError('Oversized raw blob')
            if kind==b'OSMHeader':
                h=message(payload)
                if any(v not in (b'OsmSchema-V0.6',b'DenseNodes') for v in h.get(4,[])):raise ValueError('Unsupported required PBF feature')
                header_seen=True;yield 'header',h
            elif kind==b'OSMData':
                if not header_seen:raise ValueError('PBF data before header')
                yield 'data',message(payload)
        if not header_seen:raise ValueError('Missing PBF header')

def tags(m,strings):
    keys,values=packed(m,2),packed(m,3)
    if len(keys)!=len(values):raise ValueError('PBF tag array mismatch')
    return {strings[k]:strings[v] for k,v in zip(keys,values)}

def entities(block,nodes_only):
    strings=[v.decode('utf8') for v in message(scalar(block,1,b'')).get(1,[])]
    granularity=scalar(block,17,100);lat_offset=int64(scalar(block,19));lon_offset=int64(scalar(block,20))
    for raw in block.get(2,[]):
        g=message(raw)
        if nodes_only:
            for raw_node in g.get(1,[]):
                n=message(raw_node)
                yield {'type':'node','id':signed(scalar(n,1)),
                    'lat':(lat_offset+granularity*signed(scalar(n,8)))*1e-9,
                    'lon':(lon_offset+granularity*signed(scalar(n,9)))*1e-9,'tags':tags(n,strings)}
            for raw_dense in g.get(2,[]):
                d=message(raw_dense);ids=deltas(packed(d,1));lats=deltas(packed(d,8));lons=deltas(packed(d,9));kv=packed(d,10);at=0
                if len(ids)!=len(lats) or len(ids)!=len(lons):raise ValueError('PBF dense array mismatch')
                for identity,lat,lon in zip(ids,lats,lons):
                    t={}
                    if kv:
                        while at<len(kv) and kv[at]:
                            if at+1>=len(kv):raise ValueError('Invalid dense tags')
                            t[strings[kv[at]]]=strings[kv[at+1]];at+=2
                        if at>=len(kv):raise ValueError('Missing dense tag delimiter')
                        at+=1
                    yield {'type':'node','id':identity,'lat':(lat_offset+granularity*lat)*1e-9,
                        'lon':(lon_offset+granularity*lon)*1e-9,'tags':t}
                if at!=len(kv):raise ValueError('Extra dense tags')
        else:
            for raw_way in g.get(3,[]):
                w=message(raw_way);yield {'type':'way','id':scalar(w,1),'tags':tags(w,strings),'nodes':deltas(packed(w,8))}
            for raw_relation in g.get(4,[]):
                r=message(raw_relation);roles,refs,kinds=packed(r,8),deltas(packed(r,9)),packed(r,10)
                if len(roles)!=len(refs) or len(refs)!=len(kinds) or any(k>2 for k in kinds):raise ValueError('Invalid relation members')
                yield {'type':'relation','id':scalar(r,1),'tags':tags(r,strings),'members':[
                    {'type':('node','way','relation')[k],'ref':ref,'role':strings[role]} for role,ref,k in zip(roles,refs,kinds)]}

def main():
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--bbox',nargs=4,type=float,required=True);a=p.parse_args();bbox=a.bbox
    nodes={};ways={};relations=[];timestamp=None
    for kind,block in blocks(a.source):
        if kind=='header':
            if 32 in block:timestamp=datetime.datetime.fromtimestamp(scalar(block,32),datetime.timezone.utc).isoformat().replace('+00:00','Z')
            continue
        for e in entities(block,True):
            if bbox[0]-.04<=e['lon']<=bbox[2]+.04 and bbox[1]-.04<=e['lat']<=bbox[3]+.04:nodes[e['id']]=e
    print('retained nodes',len(nodes),flush=True)
    for kind,block in blocks(a.source):
        if kind!='data':continue
        for e in entities(block,False):
            if e['type']=='way':
                if any(n in nodes for n in e['nodes']):ways[e['id']]=e
            elif e.get('tags',{}).get('type') in ('multipolygon','boundary') and e.get('tags',{}).get('boundary')!='administrative':relations.append(e)
    relations=[r for r in relations if any(m['type']=='way' and m['ref'] in ways for m in r['members'])]
    # Retain the features the offline UI consumes, rather than every suburban
    # building vertex in the province extract. All named POIs/roads, mapped
    # addresses and entrances remain; building texture stays central.
    def named_place(t):return bool(category(t) and (t.get('name') or t.get('addr:street') and t.get('addr:housenumber')))
    relations=[r for r in relations if named_place(r.get('tags',{})) or r.get('tags',{}).get('natural')=='water']
    members={m['ref'] for r in relations for m in r['members'] if m['type']=='way'}
    selected={}
    for identity,e in ways.items():
        t=e.get('tags',{})
        central_building=t.get('building') and any(n in nodes and 108.92<=nodes[n]['lon']<=108.98 and 34.25<=nodes[n]['lat']<=34.28 for n in e['nodes'])
        if t.get('highway') or named_place(t) or central_building or identity in members or t.get('natural')=='water':selected[identity]=e
    ways=selected
    needed={n for e in ways.values() for n in e['nodes']}
    nodes={identity:e for identity,e in nodes.items() if identity in needed or named_place(e.get('tags',{})) or
           e.get('tags',{}).get('entrance') or e.get('tags',{}).get('barrier') in ('gate','lift_gate','swing_gate')}
    # Materialize relation geometry for search anchors, without inventing nodes.
    for r in relations:
        for m in r['members']:
            if m['type']=='way' and m['ref'] in ways:
                m['geometry']=[{'lon':nodes[n]['lon'],'lat':nodes[n]['lat']} for n in ways[m['ref']]['nodes'] if n in nodes]
    if not nodes or not ways or not timestamp:raise ValueError('Missing regional data or snapshot timestamp')
    document={'osm3s':{'timestamp_osm_base':timestamp},'elements':list(nodes.values())+list(ways.values())+relations}
    a.output.parent.mkdir(parents=True,exist_ok=True);tmp=a.output.with_suffix('.tmp')
    tmp.write_text(json.dumps(document,ensure_ascii=False,separators=(',',':')),encoding='utf8');tmp.replace(a.output)
    with a.source.open('rb') as f:source_sha=hashlib.file_digest(f,'sha256').hexdigest()
    a.output.with_suffix('.source.json').write_text(json.dumps({'source':a.source.name,'source_sha256':source_sha,
        'url':'https://download.geofabrik.de/asia/china/shaanxi.html','snapshot':timestamp,'bbox':bbox,
        'sha256':hashlib.sha256(a.output.read_bytes()).hexdigest(),'nodes':len(nodes),'ways':len(ways),'relations':len(relations),
        'license':'ODbL-1.0'},indent=2),encoding='utf8')
    print('regional source',len(nodes),len(ways),len(relations),timestamp,flush=True)

if __name__=='__main__':main()
