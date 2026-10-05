"""Build an offline RGB565 tile pyramid and a cycling graph from original OSM.

Uses existing Pillow/NumPy, never fetches a rendered map-provider tile. The
runtime readers need neither library. Output is reproducible from retained
source hashes, projection, palette and road-access profile.
"""
import argparse, collections, hashlib, json, math, struct, time, zlib, ctypes, ctypes.util
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from map_places import collect as collect_places

BBOX=(108.78,34.12,109.15,34.46)
DETAIL=(108.87,34.20,109.05,34.37)
CORE=(108.92,34.25,108.98,34.28)
TILE=256
LEVELS=(.375,.75,1.5,3.,6.,12.,24.,48.,96.)
LATREF=(BBOX[1]+BBOX[3])/2
MX=111319.49*math.cos(math.radians(LATREF)); MY=111319.49
PALETTE={'background':'#1B272C','building':'#3C505A','building_edge':'#526874',
 'commercial':'#554D3D','industrial':'#494555','park':'#285B44',
 'forest':'#244D39','grass':'#345E42','water':'#235B79',
 'water_edge':'#367697','major':'#D4C49A','secondary':'#AFBFC4',
 'street':'#80989F','cycleway':'#53C8B1','path':'#608A7F','casing':'#162125',
 'label':'#F3F4ED','muted':'#A9BABB'}

def xy(point):return ((point[0]-BBOX[0])*MX,(BBOX[3]-point[1])*MY)
def distance(a,b):return math.hypot((a[0]-b[0])*MX,(a[1]-b[1])*MY)
def text48(value):
    data=value.encode('utf8')[:47]
    while data:
        try:data.decode('utf8');break
        except UnicodeDecodeError:data=data[:-1]
    return data.ljust(48,b'\0')

def area_kind(tags):
    return 'building' if tags.get('building') not in (None,'no') else 'water' if tags.get('natural')=='water' or tags.get('waterway')=='riverbank' else 'park' if tags.get('leisure') in ('park','garden','recreation_ground') else 'forest' if tags.get('natural')=='wood' or tags.get('landuse')=='forest' else 'grass' if tags.get('landuse') in ('grass','meadow','recreation_ground') else 'commercial' if tags.get('landuse') in ('commercial','retail') else 'industrial' if tags.get('landuse')=='industrial' else None

def closed_rings(chains):
    chains=[list(c) for c in chains if len(c)>1];result=[]
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
        if chain[0]==chain[-1]:result.append(chain)
    return result

def inside(point,ring):
    x,y=point;hit=False
    for a,b in zip(ring,ring[1:]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:hit=not hit
    return hit

def road_access(tags):
    h=tags.get('highway',''); bicycle=tags.get('bicycle','')
    if h in ('motorway','motorway_link','trunk','trunk_link','construction','proposed','steps','raceway'):
        return None
    if tags.get('area')=='yes' or tags.get('motorroad')=='yes':return None
    access=tags.get('bicycle',tags.get('vehicle',tags.get('access','yes')))
    if access in ('no','private','customers','delivery','permit','destination'):return None
    if any('conditional' in key for key in ('bicycle:conditional','access:conditional') if tags.get(key)):return None
    if h in ('footway','pedestrian','path','track') and bicycle not in ('yes','designated','permissive'):return None
    if h not in ('primary','primary_link','secondary','secondary_link','tertiary','tertiary_link','residential','unclassified','living_street','service','cycleway','footway','pedestrian','path','track'):return None
    weight=1. if h in ('cycleway','living_street') else 1.08 if h=='residential' else 1.15 if h in ('tertiary','unclassified','service') else 1.30 if h in ('secondary','secondary_link') else 1.5
    if tags.get('surface') in ('unpaved','gravel','sand','mud','dirt','ground'):weight*=1.4
    one=tags.get('oneway:bicycle')
    if one is None:one=tags.get('oneway','yes' if tags.get('junction')=='roundabout' else 'no')
    return weight,one

def build_graph(ways,nodes,out,revision):
    adjacency=collections.defaultdict(list);names=[''];name_ids={'':0}
    for way in ways.values():
        tags=way.get('tags',{});access=road_access(tags)
        if access is None:continue
        name=tags.get('name','');name_id=name_ids.setdefault(name,len(names))
        if name_id==len(names):names.append(name)
        weight,one=access
        refs=way.get('nodes',[])
        for a,b in zip(refs,refs[1:]):
            if a not in nodes or b not in nodes:continue
            pa,pb=nodes[a],nodes[b]
            if not all(BBOX[0]<=p[0]<=BBOX[2] and BBOX[1]<=p[1]<=BBOX[3] for p in (pa,pb)):continue
            d=distance(pa,pb)
            if d<.02:continue
            # Both endpoints exist even when one has no outgoing edge.
            adjacency[a];adjacency[b]
            if one!='-1':adjacency[a].append((b,d,d*weight,name_id))
            if one not in ('yes','1','true'):adjacency[b].append((a,d,d*weight,name_id))
    ordered=sorted(adjacency);indices={n:i for i,n in enumerate(ordered)}
    node_bytes=bytearray();edge_bytes=bytearray();edge_count=0
    for n in ordered:
        edges=sorted(adjacency[n]);p=nodes[n]
        node_bytes+=struct.pack('<iiII',round(p[0]*1e7),round(p[1]*1e7),edge_count,len(edges))
        for b,d,cost,name_id in edges:edge_bytes+=struct.pack('<IffI',indices[b],d,cost,name_id)
        edge_count+=len(edges)
    names_data=b''.join(text48(n) for n in names)
    header=bytearray(struct.pack('<8sIIIIIII4i',b'BGRF0001',1,revision,len(ordered),edge_count,len(names),128,128+len(node_bytes),*[round(v*1e7) for v in BBOX]).ljust(128,b'\0'))
    struct.pack_into('<II',header,52,128+len(node_bytes)+len(edge_bytes),zlib.crc32(node_bytes+edge_bytes+names_data))
    struct.pack_into('<I',header,60,zlib.crc32(header[:60]+header[64:]))
    path=out/'xian-cycle.bgraph';path.write_bytes(header+node_bytes+edge_bytes+names_data)
    return {'nodes':len(ordered),'directed_edges':edge_count,'names':len(names),'bytes':path.stat().st_size,'profile':'conservative bicycle access; no turn-restriction engine'}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--detail',type=Path);parser.add_argument('--font',type=Path,default=Path(__file__).resolve().parents[1]/'assets/fonts_src/MiSans-Semibold.ttf')
    parser.add_argument('--poi',type=Path,help='Original OSM named POIs including way/relation centers')
    parser.add_argument('--lz4-library',type=Path,help='Use an already-installed LZ4 library; never installs dependencies')
    parser.add_argument('--detail-bbox',type=float,nargs=4,metavar=('WEST','SOUTH','EAST','NORTH'),help='Metadata only: actual coverage of the supplemental source')
    a=parser.parse_args();out=a.output;out.mkdir(parents=True,exist_ok=True)
    library=str(a.lz4_library) if a.lz4_library else ctypes.util.find_library('lz4')
    lz4=ctypes.CDLL(library) if library else None
    if lz4:
        lz4.LZ4_compress_default.argtypes=(ctypes.c_void_p,ctypes.c_void_p,ctypes.c_int,ctypes.c_int)
        lz4.LZ4_compress_default.restype=ctypes.c_int
        packed=ctypes.create_string_buffer(132000)
    documents=[json.loads(a.source.read_bytes())]
    if a.detail:documents.append(json.loads(a.detail.read_bytes()))
    if a.poi:documents.append(json.loads(a.poi.read_bytes()))
    elements={}
    # Prefer the newer snapshot for duplicates; supplement only missing data.
    for doc in sorted(documents,key=lambda d:d.get('osm3s',{}).get('timestamp_osm_base','')):
        if doc.get('remark'):raise ValueError('Incomplete OSM response')
        for e in doc['elements']:
            key=(e['type'],e['id'])
            previous=elements.get(key,{})
            merged={**previous,**e}
            if previous.get('tags') or e.get('tags'):merged['tags']={**previous.get('tags',{}),**e.get('tags',{})}
            elements[key]=merged
    nodes={key[1]:(e['lon'],e['lat']) for key,e in elements.items() if key[0]=='node'}
    ways={key[1]:e for key,e in elements.items() if key[0]=='way'}
    revision=int(time.time());graph=build_graph(ways,nodes,out,revision)
    features=[];labels=[];pois=[];polygon_holes={};relation_ways=set()
    for rel in elements.values():
        tags=rel.get('tags',{});kind=area_kind(tags)
        if rel['type']!='relation' or tags.get('type')!='multipolygon' or not kind:continue
        members=[m for m in rel.get('members',[]) if m['type']=='way' and m['ref'] in ways]
        outers=closed_rings([ways[m['ref']].get('nodes',[]) for m in members if m.get('role') in ('outer','')])
        inners=closed_rings([ways[m['ref']].get('nodes',[]) for m in members if m.get('role')=='inner'])
        for refs in outers:
            if any(n not in nodes for n in refs):continue
            pts=[xy(nodes[n]) for n in refs];features.append((kind,pts,tags.get('name','')))
            holes=[[xy(nodes[n]) for n in ring] for ring in inners if all(n in nodes for n in ring)]
            polygon_holes[id(pts)]=[ring for ring in holes if inside(ring[0],pts)]
            if tags.get('name') and kind in ('water','park','forest','grass'):
                labels.append((sum(p[0] for p in pts[:-1])/(len(pts)-1),sum(p[1] for p in pts[:-1])/(len(pts)-1),tags['name'],-1))
        if outers:relation_ways.update(m['ref'] for m in members)
    for way in ways.values():
        tags=way.get('tags',{});pts=[xy(nodes[n]) for n in way.get('nodes',[]) if n in nodes]
        if len(pts)<2:continue
        name=tags.get('name','');h=tags.get('highway')
        if h and tags.get('area')!='yes' and h not in ('proposed','construction'):
            level=0 if h in ('motorway','trunk','primary','primary_link') else 1 if h in ('secondary','tertiary','secondary_link','tertiary_link') else 3 if h=='cycleway' else 4 if h in ('path','footway','pedestrian','steps','track') else 2
            features.append((10+level,pts,name))
            if name and level<3:
                segments=[(math.dist(p,q),p,q) for p,q in zip(pts,pts[1:])]
                _,p,q=max(segments)
                labels.append(((p[0]+q[0])/2,(p[1]+q[1])/2,name,level))
        if pts[0]!=pts[-1] or way['id'] in relation_ways:continue
        kind=area_kind(tags)
        if kind:
            features.append((kind,pts,name))
            if name and kind not in ('building','industrial','commercial'):
                labels.append((sum(p[0] for p in pts[:-1])/(len(pts)-1),sum(p[1] for p in pts[:-1])/(len(pts)-1),name,-1))
    for e in elements.values():
        if e['type']!='node' or not e.get('tags',{}).get('name'):continue
        tags=e['tags'];p=xy((e['lon'],e['lat']));name=tags['name']
        if 'place' in tags:labels.append((*p,name,-2))
        elif 'amenity' in tags or 'tourism' in tags:
            category=tags.get('amenity',tags.get('tourism',''))
            if category in ('hospital','clinic','school','university','museum','attraction','fuel','drinking_water','bicycle_parking','bicycle_rental','toilets','parking','police'):
                pois.append((*p,name,category))
    # POI names are a separate text layer, including polygon/relation schools.
    places=collect_places(elements)
    for place in places:
        x,y=xy((place['lon'],place['lat']))
        labels.append((x,y,place['name'],-(place['kind']+10)))
        pois.append((x,y,place['name'],'school' if place['kind']==3 else 'hospital' if place['kind']==5 else 'attraction'))
    # Index complete geometry into each pyramid level once, not on the device.
    order={'commercial':0,'industrial':0,'forest':1,'grass':1,'park':2,'water':3,'building':4}
    features.sort(key=lambda f:order.get(f[0],10))
    water_names={name for kind,pts,name in features if kind=='water' and name}
    width=(BBOX[2]-BBOX[0])*MX;height=(BBOX[3]-BBOX[1])*MY
    levels=[];total=0
    for mpp in LEVELS:
        cols=math.ceil(width/mpp/TILE);rows=math.ceil(height/mpp/TILE)
        levels.append((mpp,cols,rows,total));total+=cols*rows
    level_data=b''.join(struct.pack('<fIII',*level) for level in levels)
    directory=bytearray(total*16);payload_offset=128+len(level_data)+len(directory)
    path=out/'xian-color.btiles';temporary=path.with_suffix('.tmp')
    fonts={size:ImageFont.truetype(str(a.font),size*2) for size in (14,16,18,20)}
    start=time.perf_counter();encoded=0;label_directory=bytearray(total*8)
    label_path=out/'xian-labels.tmp'
    with temporary.open('wb') as file,label_path.open('w+b') as label_file:
        file.seek(payload_offset)
        label_file.seek(len(label_directory))
        for li,(mpp,cols,rows,first) in enumerate(levels):
            buckets=collections.defaultdict(list);lb=collections.defaultdict(list);pb=collections.defaultdict(list)
            span=mpp*TILE
            limit=CORE if mpp<1.5 else DETAIL if mpp==1.5 else BBOX
            clip_x0=max(0,int((limit[0]-BBOX[0])*MX/span)-1);clip_x1=min(cols-1,int((limit[2]-BBOX[0])*MX/span)+1)
            clip_y0=max(0,int((BBOX[3]-limit[3])*MY/span)-1);clip_y1=min(rows-1,int((BBOX[3]-limit[1])*MY/span)+1)
            for fi,(kind,pts,name) in enumerate(features):
                if isinstance(kind,int) and kind==14 and mpp>12:continue
                if kind=='building' and mpp>12:continue
                minx=max(clip_x0,int(min(p[0] for p in pts)/span));maxx=min(clip_x1,int(max(p[0] for p in pts)/span))
                miny=max(clip_y0,int(min(p[1] for p in pts)/span));maxy=min(clip_y1,int(max(p[1] for p in pts)/span))
                for ty in range(miny,maxy+1):
                    for tx in range(minx,maxx+1):buckets[ty*cols+tx].append(fi)
            # Deterministic world-space label selection prevents tile-edge seams.
            occupied=set();accepted=[];name_positions=collections.defaultdict(list)
            for x,y,name,rank in sorted(labels,key=lambda l:(l[3],l[2],l[0],l[1])):
                if rank<=-10 and mpp>(24 if rank==-14 else 12):continue
                if rank==2 and mpp>6 or rank==1 and mpp>24 or rank==0 and mpp>48 or rank==-1 and mpp>24:continue
                if rank==-2 and mpp<12:continue
                cell=(int(x/mpp/160),int(y/mpp/70))
                if cell in occupied:continue
                if any(math.hypot(x/mpp-px,y/mpp-py)<360 for px,py in name_positions[name]):continue
                occupied.add(cell);accepted.append((x,y,name,rank))
                name_positions[name].append((x/mpp,y/mpp))
            for label in accepted:
                x,y=label[:2];tx=int(x/span);ty=int(y/span)
                if 0<=tx<cols and 0<=ty<rows:lb[ty*cols+tx].append(label)
            if mpp<=6:
                for p in pois:
                    x,y=p[:2];tx=int(x/span);ty=int(y/span)
                    if 0<=tx<cols and 0<=ty<rows:pb[ty*cols+tx].append(p)
            for ty in range(rows):
                for tx in range(cols):
                    k=ty*cols+tx;cx=BBOX[0]+(tx+.5)*span/MX;cy=BBOX[3]-(ty+.5)*span/MY
                    if mpp<=1.5 and (tx<clip_x0 or tx>clip_x1 or ty<clip_y0 or ty>clip_y1):continue
                    tile_labels=lb[k][:16]
                    if tile_labels:
                        struct.pack_into('<II',label_directory,(first+k)*8,label_file.tell(),len(tile_labels))
                        for x,y,name,rank in tile_labels:
                            kind=-rank-10 if rank<=-10 else 2 if rank<0 and name in water_names else 1 if rank<0 else 0
                            label_file.write(struct.pack('<ffI48s',x,y,kind,text48(name)))
                    if not buckets[k] and not lb[k] and not pb[k]:
                        color=((0x1B>>3)<<11)|((0x27>>2)<<5)|(0x2C>>3)
                        data=struct.pack('<4H',65535,color,1,color);offset=file.tell();file.write(data)
                        struct.pack_into('<IIII',directory,(first+k)*16,offset,len(data),zlib.crc32(data),1);encoded+=1
                        continue
                    image=Image.new('RGB',(TILE*2,TILE*2),PALETTE['background']);draw=ImageDraw.Draw(image)
                    selected=[features[fi] for fi in buckets[k]]
                    def points(pts):return [((x/mpp-tx*TILE)*2,(y/mpp-ty*TILE)*2) for x,y in pts]
                    for kind,pts,name in selected:
                        if isinstance(kind,int):continue
                        pp=points(pts);holes=polygon_holes.get(id(pts),[])
                        if holes:
                            mask=Image.new('L',image.size);mask_draw=ImageDraw.Draw(mask);mask_draw.polygon(pp,fill=255)
                            for ring in holes:mask_draw.polygon(points(ring),fill=0)
                            image.paste(PALETTE[kind],(0,0,image.width,image.height),mask)
                        else:draw.polygon(pp,fill=PALETTE[kind])
                        if kind=='building':draw.line(pp,fill=PALETTE['building_edge'],width=1)
                    roads=[f for f in selected if isinstance(f[0],int)]
                    for casing in (True,False):
                        for kind,pts,name in sorted(roads,key=lambda f:-f[0]):
                            level=kind-10;meters=(13,8,5,3,1.5)[level]
                            width=max(1.2,meters/mpp) if mpp<=6 else (3.5,2.5,1.5,1.2,1)[level]
                            width=min(48,width)
                            color=PALETTE['casing'] if casing else PALETTE[('major','secondary','street','cycleway','path')[level]]
                            draw.line(points(pts),fill=color,width=max(1,round((width+(1.5 if casing else 0))*2)),joint='curve')
                    for x,y,name,category in pb[k]:
                        px=(x/mpp-tx*TILE)*2;py=(y/mpp-ty*TILE)*2
                        color='#68B7EC' if category in ('hospital','clinic','police') else '#CBAD7B' if category in ('school','university','museum') else '#63CBB0'
                        draw.ellipse((px-6,py-6,px+6,py+6),fill=color,outline='#EEF4EB',width=2)
                    # Names are stored separately and drawn upright by LVGL.
                    image=image.resize((TILE,TILE),Image.Resampling.LANCZOS)
                    rgb=np.asarray(image,dtype=np.uint16);pixels=((rgb[:,:,0]>>3)<<11)|((rgb[:,:,1]>>2)<<5)|(rgb[:,:,2]>>3)
                    values=pixels.reshape(-1).astype('<u2');starts=np.r_[0,np.flatnonzero(values[1:]!=values[:-1])+1];lengths=np.diff(np.r_[starts,len(values)])
                    if len(starts)*4<len(values)*2 and int(lengths.max())<=65535:
                        pairs=np.empty((len(starts),2),dtype='<u2');pairs[:,0]=lengths;pairs[:,1]=values[starts];data=pairs.tobytes();encoding=1
                    else:data=values.tobytes();encoding=0
                    if lz4:
                        raw_pixels=values.tobytes();compressed=lz4.LZ4_compress_default(raw_pixels,packed,len(raw_pixels),len(packed))
                        if compressed and compressed<len(data):data=packed.raw[:compressed];encoding=2
                    offset=file.tell();file.write(data)
                    if offset>0xffffffff:raise ValueError('Package exceeds 32-bit storage offsets')
                    struct.pack_into('<IIII',directory,(first+k)*16,offset,len(data),zlib.crc32(data),encoding);encoded+=1
                if ty%20==0:print('level %.1f m/px: row %d/%d, %d tiles, %.1f MiB'%(mpp,ty,rows,encoded,file.tell()/1048576),flush=True)
            print('level complete: %.1f m/px'%mpp,flush=True)
        label_file.seek(0);label_file.write(label_directory);label_file.flush();label_file.seek(0)
        label_offset=file.tell();label_data=label_file.read();file.write(label_data)
        header=bytearray(struct.pack('<8sIIII4iIffI',b'BTIL0002',2,revision,TILE,len(levels),*[round(v*1e7) for v in BBOX],round(LATREF*1e7),MX,MY,total).ljust(128,b'\0'))
        struct.pack_into('<III',header,72,label_offset,len(label_data),zlib.crc32(label_data))
        struct.pack_into('<II',header,60,128+len(level_data),zlib.crc32(level_data+directory))
        struct.pack_into('<I',header,68,zlib.crc32(header[:68]+header[72:]))
        file.seek(0);file.write(header+level_data+directory)
    temporary.replace(path)
    label_path.unlink()
    extra=Path(__file__).resolve().parent/'ui_extra_characters.txt'
    glyphs=extra.read_text(encoding='utf8') if extra.exists() else ''
    extra.write_text(''.join(sorted(set(glyphs+''.join(label[2] for label in labels))))+'\n',encoding='utf8')
    manifest={'format':'BTIL0002','name':'西安彩色离线地图','bbox_lon_lat':BBOX,'detail_bbox_lon_lat':DETAIL,'levels_mpp':LEVELS,'tile_size':TILE,'tiles':encoded,'bytes':path.stat().st_size,'palette':PALETTE,'graph':graph,'source_sha256':hashlib.sha256(a.source.read_bytes()).hexdigest(),'detail_sha256':hashlib.sha256(a.detail.read_bytes()).hexdigest() if a.detail else None,'osm_timestamp':[d.get('osm3s',{}).get('timestamp_osm_base') for d in documents],'license':'ODbL-1.0','attribution':'© OpenStreetMap contributors','build_seconds':round(time.perf_counter()-start,2),'limitations':['Original OSM completeness varies; no invented lane or traffic-light data.','Highest-resolution tiles cover the detail rectangle; other areas use next level.','Routing is conservative road-graph navigation, not a certified commercial engine.']}
    manifest.update({'coordinate_system':'WGS84','highest_resolution_bbox_lon_lat':CORE,
        'poi_source_sha256':hashlib.sha256(a.poi.read_bytes()).hexdigest() if a.poi else None,
        'named_poi_count':len(places),'poi_kind_counts':dict(collections.Counter(str(p['kind']) for p in places)),
        'resolution_1_5_bbox_lon_lat':DETAIL,
        'base_building_data_bbox_lon_lat':[108.92,34.25,108.98,34.28],
        'supplemental_source_bbox_lon_lat':a.detail_bbox,'label_layer':'upright cached labels, screen-space collision avoidance',
        'polygon_layer':'closed ways and multipolygon outer rings with holes',
        'package_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'graph_sha256':hashlib.sha256((out/'xian-cycle.bgraph').read_bytes()).hexdigest()})
    (out/'color_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:manifest[k] for k in ('tiles','bytes','build_seconds','graph')},ensure_ascii=False),flush=True)
if __name__=='__main__':main()
