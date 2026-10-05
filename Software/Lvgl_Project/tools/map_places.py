"""Extract named real places from OSM nodes, ways and relations."""
import math

BBOX=(108.78,34.12,109.15,34.46)
def category(tags):
    a=tags.get('amenity','')
    if a in ('school','university','college'):return 3,'学校'
    if a in ('hospital','clinic'):return 5,'医疗'
    if tags.get('tourism') in ('museum','attraction','viewpoint') or tags.get('historic'):return 4,'地标'
    if tags.get('railway') in ('station','halt') or tags.get('public_transport')=='station':return 6,'车站'
    if tags.get('leisure') in ('park','garden'):return 1,'公园'
    if a in ('fuel','drinking_water','bicycle_parking','bicycle_rental','toilets','police','library') or tags.get('shop') in ('bicycle','convenience','supermarket'):return 7,'地点'
    return None

def anchor(element,nodes):
    if element['type']=='node':return element.get('lon'),element.get('lat')
    if element.get('center'):return element['center']['lon'],element['center']['lat']
    points=[nodes[n] for n in element.get('nodes',[]) if n in nodes]
    if not points:return None
    if len(points)>2 and points[0]==points[-1]:points=points[:-1]
    # A display/search anchor only. This does not claim a public entrance.
    return sum(p[0] for p in points)/len(points),sum(p[1] for p in points)/len(points)

def collect(elements,roads=False):
    nodes={e['id']:(e['lon'],e['lat']) for e in elements.values() if e['type']=='node'}
    found=[];seen={}
    for e in sorted(elements.values(),key=lambda e:(e['type'],e['id'])):
        t=e.get('tags',{});name=t.get('name','').strip()
        if not name:continue
        kind=category(t)
        if roads and t.get('highway') and t.get('highway') not in ('proposed','construction'):kind=(0,'道路')
        if not kind:continue
        p=anchor(e,nodes)
        if not p or p[0] is None or not (BBOX[0]<=p[0]<=BBOX[2] and BBOX[1]<=p[1]<=BBOX[3]):continue
        key=(kind[0],name);old=seen.setdefault(key,[])
        if any(math.hypot((p[0]-q[0])*92000,(p[1]-q[1])*111319)<(250 if kind[0]==0 else 80) for q in old):continue
        old.append(p)
        aliases=[]
        for field in ('alt_name','short_name','official_name','name:en'):
            aliases.extend(x.strip() for x in t.get(field,'').split(';') if x.strip())
        detail=kind[1]
        if t.get('addr:street'):detail+=' · '+t['addr:street']+t.get('addr:housenumber','')
        found.append({'name':name,'aliases':aliases,'lon':p[0],'lat':p[1],'kind':kind[0],
                      'detail':detail,'osm_type':e['type'],'osm_id':e['id'],
                      'entrance':bool(t.get('entrance'))})
    return found
