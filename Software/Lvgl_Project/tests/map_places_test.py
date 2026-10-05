"""Regression scenarios: missing campus geometry, ambiguous aliases and doors."""
import sys, unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from map_places import collect, institution_aliases
from build_place_index import display_name

class PlaceExtraction(unittest.TestCase):
    def test_distinct_campuses_and_base_alias(self):
        es={}
        for i,(suffix,x) in enumerate((('未央校区',108.98),('太华路校区',108.97))):
            e={'type':'way','id':i+1,'tags':{'amenity':'university','name':'陕西科技大学（'+suffix+'）'},
               'geometry':[{'lon':x,'lat':34.38},{'lon':x+.001,'lat':34.38},{'lon':x+.001,'lat':34.381},{'lon':x,'lat':34.38}]}
            es[('way',e['id'])]=e
        ps=collect(es);self.assertEqual(len(ps),2)
        for p in ps:self.assertIn('陕西科技大学',p['aliases']);self.assertIn('skd',p['aliases'])
        aliases=[];institution_aliases('西安交通大学附属中学',aliases)
        self.assertNotIn('西安交通大学',aliases)

    def test_nearby_private_door_is_not_a_campus_entrance(self):
        school={'type':'way','id':1,'nodes':[10,11,12,10],'tags':{'amenity':'university','name':'测试大学'},
                'geometry':[{'lon':108.95,'lat':34.30},{'lon':108.96,'lat':34.30},{'lon':108.96,'lat':34.31},{'lon':108.95,'lat':34.30}]}
        private={'type':'node','id':10,'lon':108.95,'lat':34.30,'tags':{'entrance':'main','access':'private'}}
        nearby={'type':'node','id':13,'lon':108.951,'lat':34.30,'tags':{'entrance':'main'}}
        es={('way',1):school,('node',10):private,('node',13):nearby}
        p=collect(es)[0];self.assertFalse(p['entrance']);self.assertIsNone(p['route'])
        private['tags']['access']='yes';p=collect(es)[0]
        self.assertTrue(p['entrance']);self.assertEqual(p['route'],(108.95,34.30))

    def test_address_and_geofence(self):
        e={'type':'node','id':1,'lon':108.95,'lat':34.30,'tags':{'addr:street':'学府路','addr:housenumber':'1号'}}
        region={'未央区':[[(108.94,34.29),(108.96,34.29),(108.96,34.31),(108.94,34.31),(108.94,34.29)]]}
        ps=collect({('node',1):e},regions=region);self.assertEqual(ps[0]['name'],'学府路1号')
        self.assertIn('未央区',ps[0]['detail']);e['lon']=108.98
        self.assertEqual(collect({('node',1):e},regions=region),[])

    def test_identical_names_with_different_addresses_stay_distinct(self):
        es={}
        for i in (1,2):es[('node',i)]={'type':'node','id':i,'lon':108.95,'lat':34.30,'tags':{'name':'同名便利店','shop':'convenience','addr:street':'学府路','addr:housenumber':str(i)}}
        self.assertEqual(len(collect(es)),2)
        label=display_name('非常长的学校名称用于检验不会把不同校区名称全部截断（未央校区）')
        self.assertLessEqual(len(label.encode('utf8')),63);self.assertTrue(label.endswith('（未央校区）'))

if __name__=='__main__':unittest.main()
