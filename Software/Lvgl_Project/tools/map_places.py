"""Extract named real places from OSM nodes, ways and relations."""
import math, re

BBOX=(108.78,34.12,109.15,34.46)
INSTITUTIONS={
    '陕西科技大学':('陕科大','陕西科大','skd','sxkd'),
    '西安交通大学':('西交大','西安交大','xajd'),
    '西北工业大学':('西工大','西北工大','xgd'),
    '西安电子科技大学':('西电','西电科大'),
    '西北大学':('西大','西北大'),
    '陕西师范大学':('陕师大','陕西师大','ssd'),
    '西安理工大学':('西理工','西安理工'),
    '西安工业大学':('西安工大','西安工业'),
    '长安大学':('长大','长安大'),
    '西安建筑科技大学':('西建大','西安建大','xajd'),
}
def category(tags):
    a=tags.get('amenity','')
    if a in ('school','university','college','kindergarten') or tags.get('landuse')=='education':return 3,'学校'
    if a in ('hospital','clinic','doctors','dentist','pharmacy'):return 5,'医疗'
    if tags.get('tourism') in ('museum','attraction','viewpoint') or tags.get('historic'):return 4,'地标'
    if tags.get('railway')=='subway_entrance':return 6,'地铁出入口'
    if tags.get('highway')=='bus_stop' or tags.get('public_transport')=='platform' and tags.get('bus')=='yes':return 6,'公交站'
    if tags.get('railway') in ('station','halt','tram_stop') or tags.get('public_transport') in ('station','platform'):return 6,'地铁站' if tags.get('station')=='subway' or tags.get('subway')=='yes' else '车站'
    if tags.get('leisure') in ('park','garden'):return 1,'公园'
    if tags.get('landuse')=='residential' or tags.get('building')=='apartments':return 7,'住宅'
    if a or tags.get('shop') or tags.get('tourism') or tags.get('leisure') or tags.get('building') in ('commercial','retail'):return 7,'地点'
    if tags.get('addr:street') and tags.get('addr:housenumber'):return 7,'地址'
    return None

def geometry(element,nodes):
    return [(p['lon'],p['lat']) for p in element.get('geometry',[]) if p] or [nodes[n] for n in element.get('nodes',[]) if n in nodes]

def anchor(element,nodes):
    if element['type']=='node':return element.get('lon'),element.get('lat')
    if element.get('center'):return element['center']['lon'],element['center']['lat']
    points=geometry(element,nodes)
    if not points and element['type']=='relation':points=[(p['lon'],p['lat']) for m in element.get('members',[]) for p in m.get('geometry',[]) if p]
    if not points:return None
    # Bounding-box center is a display location, never an entrance claim.
    return (min(p[0] for p in points)+max(p[0] for p in points))/2,(min(p[1] for p in points)+max(p[1] for p in points))/2

def distance(a,b):return math.hypot((a[0]-b[0])*92000,(a[1]-b[1])*111319)

def inside(point,ring):
    x,y=point;hit=False
    for a,b in zip(ring,ring[1:]):
        if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:hit=not hit
    return hit

def rings(chains):
    chains=[list(c) for c in chains if len(c)>1];out=[]
    while chains:
        chain=chains.pop()
        while chain[0]!=chain[-1]:
            for i,c in enumerate(chains):
                if chain[-1]==c[0]:chain+=c[1:]
                elif chain[-1]==c[-1]:chain+=c[-2::-1]
                elif chain[0]==c[-1]:chain=c[:-1]+chain
                elif chain[0]==c[0]:chain=c[:0:-1]+chain
                else:continue
                chains.pop(i);break
            else:raise ValueError('Open administrative/campus boundary')
        out.append(chain)
    return out

def district_regions(document):
    return {e['tags']['name']:rings([[(p['lon'],p['lat']) for p in m['geometry'] if p]
        for m in e['members'] if m.get('role') in ('outer','inner','') and m.get('geometry')]) for e in document['elements']}

class DistrictLookup:
    """Cache interior grid cells; resolve boundary cells against actual rings."""
    def __init__(self,regions):
        self.regions=regions;self.cache={};self.boundary=set();self.bounds={}
        for name,polygons in regions.items():
            self.bounds[name]=[(min(p[0] for p in r),min(p[1] for p in r),max(p[0] for p in r),max(p[1] for p in r)) for r in polygons]
            for ring in polygons:
                for a,b in zip(ring,ring[1:]):
                    for x in range(math.floor(min(a[0],b[0])*100),math.floor(max(a[0],b[0])*100)+1):
                        for y in range(math.floor(min(a[1],b[1])*100),math.floor(max(a[1],b[1])*100)+1):self.boundary.add((x,y))
    def exact(self,p):
        for name,polygons in self.regions.items():
            if sum(inside(p,r) for r,b in zip(polygons,self.bounds[name]) if b[0]<=p[0]<=b[2] and b[1]<=p[1]<=b[3])%2:return name
        return None
    def __call__(self,p):
        key=math.floor(p[0]*100),math.floor(p[1]*100)
        if key in self.boundary:return self.exact(p)
        if key not in self.cache:self.cache[key]=self.exact(((key[0]+.5)/100,(key[1]+.5)/100))
        return self.cache[key]

def institution_aliases(name,aliases):
    # Strip explicit campus qualifiers only. Affiliated schools stay distinct.
    base=re.sub(r'[（(][^）)]*校区[）)]','',name).strip()
    match=re.fullmatch(r'(.+(?:大学|学院|学校))[^附属]*校区',base)
    if match:base=match[1]
    if base!=name:aliases.append(base)
    short=INSTITUTIONS.get(base,());aliases.extend(short)
    campus=re.search(r'[（(]([^（）()]*校区)[）)]',name)
    suffix=campus[1] if campus else name[len(base):] if base!=name else ''
    if suffix:
        aliases.extend(alias+suffix for alias in short)
        aliases.extend(alias+suffix.replace('校区','') for alias in short)
    if base.endswith('大学'):aliases.append(base[:-2]+'大')
    if '陕西' in name:aliases.append(name.replace('陕西','shaanxi'))
    return base

def collect(elements,roads=False,bbox=BBOX,regions=None):
    lookup=DistrictLookup(regions) if regions else None
    nodes={e['id']:(e['lon'],e['lat']) for e in elements.values() if e['type']=='node' and 'lon' in e}
    gates=[e for e in elements.values() if e['type']=='node' and (e.get('tags',{}).get('entrance') in ('yes','main') or e.get('tags',{}).get('barrier') in ('gate','lift_gate','swing_gate'))
           and e.get('tags',{}).get('access') not in ('no','private') and e.get('tags',{}).get('bicycle')!='no']
    gate_ids={g['id']:g for g in gates};gate_cells={}
    for g in gates:gate_cells.setdefault((round(g['lon']*1e5),round(g['lat']*1e5)),[]).append(g)
    found=[];seen={}
    for e in sorted(elements.values(),key=lambda e:(e['type'],e['id'])):
        t=e.get('tags',{});name=t.get('name','').strip()
        if not name and t.get('addr:street') and t.get('addr:housenumber'):name=t['addr:street']+t['addr:housenumber']
        if not name:continue
        kind=category(t)
        if roads and e['type']=='way' and t.get('highway') and t.get('highway') not in ('proposed','construction'):kind=(0,'道路')
        if not kind:continue
        p=anchor(e,nodes)
        if not p or p[0] is None or not (bbox[0]<=p[0]<=bbox[2] and bbox[1]<=p[1]<=bbox[3]):continue
        district=lookup(p) if lookup else None
        if regions and not district:continue
        key=(kind[0],name,t.get('addr:street',''),t.get('addr:housenumber',''),t.get('addr:floor',t.get('level','')));old=seen.setdefault(key,[])
        if any(distance(p,q)<(250 if kind[0]==0 else 10 if kind[0]==6 else 50) for q in old):continue
        old.append(p)
        aliases=[]
        for field in ('alt_name','short_name','official_name','name:en','name:zh'):
            aliases.extend(x.strip() for x in t.get(field,'').split(';') if x.strip())
        # Stable OSM object in Weiyang; preserve its original 西安校区 label.
        if e['type']=='way' and e['id']==293805614 and district=='未央区' and '陕西科技大学' in name:
            aliases.append(name);name='陕西科技大学（未央校区）'
        base=institution_aliases(name,aliases) if kind[0]==3 else name
        address=t.get('addr:street','')+t.get('addr:housenumber','')
        if address:aliases.append(address)
        detail=' · '.join(x for x in (kind[1],district or t.get('addr:district',''),address) if x)
        if kind[0]==6 and t.get('local_ref'):detail+=' · '+t['local_ref']
        route=None;entrance=False
        if t.get('entrance') in ('main','yes') and t.get('access') not in ('no','private') and t.get('bicycle')!='no':route=p;entrance=True
        # Associate only a gate connected to the boundary, never a nearby door.
        refs=set(e.get('nodes',[]));boundary=geometry(e,nodes)
        candidates={i:gate_ids[i] for i in refs if i in gate_ids} if kind[0] in (1,3,5,7) and e['type']!='node' else {}
        if kind[0] in (1,3,5,7) and e['type']!='node':
            for v in boundary:
                cx,cy=round(v[0]*1e5),round(v[1]*1e5)
                for dx in (-1,0,1):
                    for dy in (-1,0,1):
                        for g in gate_cells.get((cx+dx,cy+dy),[]):
                            if distance((g['lon'],g['lat']),v)<1:candidates[g['id']]=g
        if candidates:
            gate=min(candidates.values(),key=lambda g:(g['tags'].get('entrance')!='main',distance(p,(g['lon'],g['lat'])),g['id']))
            route=(gate['lon'],gate['lat']);entrance=True
        found.append({'name':name,'aliases':sorted(set(aliases)),'base':base,'lon':p[0],'lat':p[1],'kind':kind[0],
                      'detail':detail,'osm_type':e['type'],'osm_id':e['id'],'entrance':entrance,
                      'route':route,'district':district})
    return found
