"""OSM Overpass JSON -> bounded, indexed offline vector package (stdlib only).

Input is original OSM data, not rendered online tiles. Coordinates remain WGS84.
Closed simple ways are triangulated. Multipolygons with holes are deliberately
not filled: guessing holes creates false water/park/building geometry.
"""
import argparse, collections, datetime, hashlib, heapq, json, math, struct, zlib, io
from pathlib import Path

BBOX=(108.78,34.12,109.15,34.46)
BUILDING_BBOX=(108.92,34.25,108.98,34.28)
CELL=0.01
def atomic_write(path,data):
    temporary=path.with_suffix(path.suffix+'.tmp');temporary.write_bytes(data);temporary.replace(path)
def distance(a,b):
    return math.hypot((a[0]-b[0])*111319.49*math.cos(math.radians((a[1]+b[1])/2)),(a[1]-b[1])*111319.49)
def text48(s):
    b=s.encode('utf-8')[:47]
    while True:
        try: b.decode('utf-8'); break
        except UnicodeDecodeError: b=b[:-1]
    return b.ljust(48,b'\0')
def simplify(p,tolerance):
    if len(p)<=2:return p
    a,b=p[0],p[-1]; dx=(b[0]-a[0])*92000;dy=(b[1]-a[1])*111319.49
    den=dx*dx+dy*dy;best=0;idx=0
    for i,c in enumerate(p[1:-1],1):
        x=(c[0]-a[0])*92000;y=(c[1]-a[1])*111319.49
        t=max(0,min(1,(x*dx+y*dy)/den)) if den else 0
        d=math.hypot(x-t*dx,y-t*dy)
        if d>best:best,idx=d,i
    if best<=tolerance:return [a,b]
    return simplify(p[:idx+1],tolerance)[:-1]+simplify(p[idx:],tolerance)
def cross(a,b,c):return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
def triangles(points):
    p=list(dict.fromkeys(points[:-1] if points[0]==points[-1] else points))
    if len(p)<3:return []
    if sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(p,p[1:]+p[:1]))<0:p.reverse()
    out=[];attempts=0
    while len(p)>3 and attempts<len(p)*len(p):
        found=False
        for i,b in enumerate(p):
            a,c=p[i-1],p[(i+1)%len(p)]
            if cross(a,b,c)<=1e-15:continue
            if any(cross(a,b,q)>=-1e-15 and cross(b,c,q)>=-1e-15 and cross(c,a,q)>=-1e-15 for j,q in enumerate(p) if j not in ((i-1)%len(p),i,(i+1)%len(p))):continue
            out.append([a,b,c]);p.pop(i);found=True;break
        if not found:return [] # no false fill for invalid/self-intersecting ways
        attempts+=1
    if len(p)==3 and cross(*p)>1e-15:out.append(p)
    return out
def record(kind,level,name,points):
    xs=[round(p[0]*1e7) for p in points];ys=[round(p[1]*1e7) for p in points]
    return struct.pack('<BBH4i',kind,level,len(points),min(xs),min(ys),max(xs),max(ys))+text48(name)+b''.join(struct.pack('<ii',x,y) for x,y in zip(xs,ys))
def polygon_parts(pts):
    p=pts[:-1] if pts[0]==pts[-1] else pts
    if 3<=len(p)<=6:
        signs=[cross(p[i-1],p[i],p[(i+1)%len(p)]) for i in range(len(p))]
        if any(abs(s)>1e-15 for s in signs) and (all(s>=0 for s in signs) or all(s<=0 for s in signs)):return [p]
    return triangles(pts)
def clip_points(points,polygon,bbox=None):
    if bbox is None:bbox=BBOX
    if not polygon:
        a,b=points;dx=b[0]-a[0];dy=b[1]-a[1];lo,hi=0.,1.
        for p,q in ((-dx,a[0]-bbox[0]),(dx,bbox[2]-a[0]),(-dy,a[1]-bbox[1]),(dy,bbox[3]-a[1])):
            if p==0:
                if q<0:return []
            elif p<0:lo=max(lo,q/p)
            else:hi=min(hi,q/p)
        return [(a[0]+lo*dx,a[1]+lo*dy),(a[0]+hi*dx,a[1]+hi*dy)] if lo<=hi else []
    p=points
    for axis,bound,greater in ((0,bbox[0],True),(0,bbox[2],False),(1,bbox[1],True),(1,bbox[3],False)):
        result=[]
        for a,b in zip(p[-1:]+p[:-1],p):
            ina=a[axis]>=bound if greater else a[axis]<=bound;inb=b[axis]>=bound if greater else b[axis]<=bound
            if ina!=inb:
                t=(bound-a[axis])/(b[axis]-a[axis]);result.append((a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1])))
            if inb:result.append(b)
        p=result
    return list(dict.fromkeys(p))
def main():
    global BBOX
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--bbox',type=float,nargs=4,default=BBOX)
    args=parser.parse_args();BBOX=tuple(args.bbox);out=args.output;out.mkdir(parents=True,exist_ok=True)
    raw=args.source.read_bytes();data=json.loads(raw)
    if data.get('remark'):raise ValueError('Overpass returned incomplete/error response: '+data['remark'])
    nodes={e['id']:(e['lon'],e['lat']) for e in data['elements'] if e['type']=='node'}
    ways={e['id']:e for e in data['elements'] if e['type']=='way'}
    holes=set();outer_tags={};joined=[]
    for rel in data['elements']:
        if rel['type']!='relation' or rel.get('tags',{}).get('type')!='multipolygon':continue
        members=[m for m in rel.get('members',[]) if m['type']=='way']
        if any(m.get('role')=='inner' for m in members):
            holes.update(m['ref'] for m in members);continue
        chains=[list(ways[m['ref']]['nodes']) for m in members if m.get('role') in ('outer','') and m['ref'] in ways]
        while chains:
            chain=chains.pop();changed=True
            while changed and chain[0]!=chain[-1]:
                changed=False
                for i,c in enumerate(chains):
                    if chain[-1]==c[0]:chain+=c[1:]
                    elif chain[-1]==c[-1]:chain+=c[-2::-1]
                    elif chain[0]==c[-1]:chain=c[:-1]+chain
                    elif chain[0]==c[0]:chain=c[:0:-1]+chain
                    else:continue
                    chains.pop(i);changed=True;break
            if chain[0]==chain[-1]:joined.append({'id':rel['id'],'nodes':chain,'tags':rel.get('tags',{})})
    cols=round((BBOX[2]-BBOX[0])/CELL);rows=round((BBOX[3]-BBOX[1])/CELL)
    tiles=collections.defaultdict(list);counts=collections.Counter();names=set();graph=collections.defaultdict(list)
    def add(kind,level,name,pts):
        pts=clip_points(pts,kind!=0)
        if not pts:return
        if kind and len(pts)<3:return
        if kind and abs(sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(pts,pts[1:]+pts[:1])))<1e-13:return
        if not kind and distance(*pts)<.05:return
        if len(pts)>6:
            for tri in triangles(pts):add(kind,level,name,tri)
            return
        minx,miny=min(p[0] for p in pts),min(p[1] for p in pts);maxx,maxy=max(p[0] for p in pts),max(p[1] for p in pts)
        if maxx<BBOX[0] or minx>BBOX[2] or maxy<BBOX[1] or miny>BBOX[3]:return
        payload=record(kind,level,name,pts);counts[['road','building','park','water'][kind]]+=1
        if name:names.add(name)
        for y in range(max(0,int((miny-BBOX[1])/CELL)),min(rows-1,int((maxy-BBOX[1])/CELL))+1):
            for x in range(max(0,int((minx-BBOX[0])/CELL)),min(cols-1,int((maxx-BBOX[0])/CELL))+1):tiles[y*cols+x].append(payload)
    major={'motorway','trunk','primary','secondary','motorway_link','trunk_link','primary_link','secondary_link'}
    mid={'tertiary','tertiary_link','residential','unclassified','living_street'}
    for way in list(ways.values())+joined:
        tags=way.get('tags',{});refs=way['nodes'];pts=[nodes[n] for n in refs if n in nodes]
        if len(pts)!=len(refs) or len(pts)<2:continue
        name=tags.get('name:zh',tags.get('name',''));h=tags.get('highway')
        if h and tags.get('area')!='yes' and h not in ('construction','proposed'):
            level=0 if h in major else 1 if h in mid else 2
            simple=simplify(pts,2)
            for a,b in zip(simple,simple[1:]):add(0,level,name,[a,b])
            # This graph is for host-side preparation only; it is not a firmware planner.
            allowed=h in mid|{'primary','secondary','cycleway'} and tags.get('access') not in ('no','private') and tags.get('bicycle')!='no' and tags.get('motorroad')!='yes'
            if allowed:
                oneway=tags.get('oneway:bicycle',tags.get('oneway','no'))
                if tags.get('junction')=='roundabout' and oneway=='no':oneway='yes'
                for a,b in zip(refs,refs[1:]):
                    d=distance(nodes[a],nodes[b])
                    if oneway!='-1':graph[a].append((b,d,name))
                    if oneway not in ('yes','1','true'):graph[b].append((a,d,name))
        if refs[0]!=refs[-1] or way['id'] in holes:continue
        kind=1 if 'building' in tags and tags['building']!='no' else 3 if tags.get('natural')=='water' or 'water' in tags or tags.get('waterway')=='riverbank' else 2 if tags.get('leisure') in ('park','garden','recreation_ground') or tags.get('landuse') in ('forest','grass','recreation_ground') or tags.get('natural') in ('wood','grassland') else None
        if kind:
            clipped=clip_points(pts[:-1],True,BUILDING_BBOX if kind==1 else BBOX)
            if len(clipped)<3:continue
            clipped=clipped+[clipped[0]]
            ts=polygon_parts(clipped if len(clipped)<20 else simplify(clipped,1 if kind==1 else 5))
            if not ts:counts['omitted_invalid_polygons']+=1
            for tri in ts:add(kind,0,name,tri)
    if counts['road']<100:raise ValueError('Insufficient real road data')
    directory=[];payload=bytearray();offset=128+cols*rows*12
    for i in range(cols*rows):
        chunk=b''.join(sorted(set(tiles[i])))
        directory.append(struct.pack('<III',offset+len(payload),len(chunk),zlib.crc32(chunk)))
        payload+=chunk
    revision=int(datetime.datetime.now(datetime.timezone.utc).timestamp())
    header=bytearray(struct.pack('<8sII4iIII',b'BMAP0001',1,revision,*[round(v*1e7) for v in BBOX],round(CELL*1e7),cols,rows).ljust(128,b'\0'))
    header[48:64]=data.get('osm3s',{}).get('timestamp_osm_base','unknown')[:10].encode().ljust(16,b'\0')
    header[64:112]=text48('西安城区基础地图')
    struct.pack_into('<I',header,44,zlib.crc32(header[:44]+header[48:]))
    package=header+b''.join(directory)+payload
    # A short continuous urban route, based exclusively on actual OSM node connectivity.
    start_target=(108.935,34.258);end_target=(108.965,34.270)
    def nearest(target):return min(graph,key=lambda n:distance(nodes[n],target))
    start,end=nearest(start_target),nearest(end_target);queue=[(0,start)];cost={start:0};parent={}
    while queue:
        d,n=heapq.heappop(queue)
        if d!=cost[n]:continue
        if n==end:break
        for nxt,w,name in graph[n]:
            nd=d+w
            if nd<cost.get(nxt,1e99):cost[nxt]=nd;parent[nxt]=(n,name);heapq.heappush(queue,(nd,nxt))
    if end not in cost:raise ValueError('Urban route endpoints disconnected')
    path=[end];roadnames=[]
    while path[-1]!=start:
        p,name=parent[path[-1]];roadnames.append(name);path.append(p)
    path.reverse();roadnames.reverse();roadnames.append(roadnames[-1]);routepts=[nodes[n] for n in path]
    cum=[0.0]
    for a,b in zip(routepts,routepts[1:]):cum.append(cum[-1]+distance(a,b))
    route_data=b''.join(struct.pack('<iif',round(p[0]*1e7),round(p[1]*1e7),d)+text48(name) for p,d,name in zip(routepts,cum,roadnames))
    route=(struct.pack('<8sIIfI',b'BRTE0001',1,1,cum[-1],len(path))+text48('西安城区骑行')+struct.pack('<I',zlib.crc32(route_data))).ljust(128,b'\0')+route_data
    if len(path)>512:raise ValueError('Route exceeds bounded route capacity; choose a shorter validation route')
    import xml.etree.ElementTree as ET
    gpx=ET.Element('gpx',version='1.1',creator='BikeComputer offline preparation',xmlns='http://www.topografix.com/GPX/1/1');rte=ET.SubElement(gpx,'rte');ET.SubElement(rte,'name').text='西安城区骑行（规划路线）'
    for p in routepts:ET.SubElement(rte,'rtept',lat=str(p[1]),lon=str(p[0]))
    gpx_bytes=io.BytesIO();ET.ElementTree(gpx).write(gpx_bytes,encoding='utf-8',xml_declaration=True)
    manifest={'format':'BMAP0001','coordinate_system':'WGS84','name':'西安城区基础地图','bbox_lon_lat':BBOX,'building_detail_bbox_lon_lat':[108.92,34.25,108.98,34.28],'source':'https://overpass.private.coffee/api/interpreter','osm_timestamp':data.get('osm3s',{}).get('timestamp_osm_base'),'downloaded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'license':'ODbL-1.0','attribution':'© OpenStreetMap contributors','copyright_url':'https://www.openstreetmap.org/copyright','raw_sha256':hashlib.sha256(raw).hexdigest(),'package_sha256':hashlib.sha256(package).hexdigest(),'package_bytes':len(package),'tiles':cols*rows,'max_tile_bytes':max(len(b''.join(set(v))) for v in tiles.values()),'features':dict(counts),'route':{'name':'西安城区骑行','distance_m':round(cum[-1]),'points':len(path),'start':nodes[start],'end':nodes[end],'source':'host-side Dijkstra on cycling-allowed OSM roads','ascent_available':False,'position_source':'simulated route replay, not GNSS','speed_kmh':18},'limitations':['urban rectangular coverage, not complete Xi’an administrative region','building detail only in central coverage rectangle','multipolygons with holes omitted rather than incorrectly filled','routing legality and real road availability require field validation']}
    atomic_write(out/'xian-urban.bmap',package)
    atomic_write(out/'xian-city.broute',route)
    atomic_write(out/'xian-city.gpx',gpx_bytes.getvalue())
    characters=set(''.join(names|set(roadnames)))
    atomic_write(out/'map_characters.txt',(''.join(sorted(c for c in characters if '\u4e00'<=c<='\u9fff' or '\u3001'<=c<='\u303f' or '\uff01'<=c<='\uff60'))+'\n').encode('utf-8'))
    atomic_write(out/'manifest.json',json.dumps(manifest,ensure_ascii=False,indent=2).encode('utf-8'))
    print(json.dumps({'features':dict(counts),'package_bytes':len(package),'route_m':round(cum[-1]),'route_points':len(path),'names':len(names),'max_tile_bytes':manifest['max_tile_bytes']},ensure_ascii=False))
if __name__=='__main__':main()
