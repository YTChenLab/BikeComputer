import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from import_osm_pbf import entities,message,varint

def v(n):
    out=bytearray()
    while n>=128:out.append((n&127)|128);n>>=7
    out.append(n);return bytes(out)
def field(k,b):return v(k*8+2)+v(len(b))+b
def scalar(k,n):return v(k*8)+v(n)
def zz(n):return 2*n if n>=0 else -2*n-1

class PbfRegression(unittest.TestCase):
    def test_dense_negative_deltas_offsets_and_tag_boundaries(self):
        # Spec fixture: ids 10,9; lat 100,99; lon 200,201; only first has a tag.
        strings=b''.join(field(1,s) for s in (b'',b'name',b'test'))
        dense=field(1,v(20)+v(1))+field(8,v(200)+v(1))+field(9,v(400)+v(2))+field(10,b'\x01\x02\x00\x00')
        block=message(field(1,strings)+field(2,field(2,dense))+scalar(17,1000)+scalar(19,1000000000)+scalar(20,2000000000))
        es=list(entities(block,True));self.assertEqual([e['id'] for e in es],[10,9])
        self.assertAlmostEqual(es[0]['lat'],1.0001);self.assertAlmostEqual(es[1]['lat'],1.000099)
        self.assertAlmostEqual(es[1]['lon'],2.000201);self.assertEqual(es[0]['tags'],{'name':'test'});self.assertEqual(es[1]['tags'],{})
    def test_simple_node_and_way_signed_coordinates_and_refs(self):
        strings=field(1,b'')
        node=scalar(1,zz(10))+scalar(8,zz(-342000000))+scalar(9,zz(1089500000))
        way=scalar(1,50)+field(8,v(zz(10))+v(zz(-1)))
        block=message(field(1,strings)+field(2,field(1,node)+field(3,way)))
        e=list(entities(block,True))[0];self.assertAlmostEqual(e['lat'],-34.2);self.assertAlmostEqual(e['lon'],108.95)
        self.assertEqual(list(entities(block,False))[0]['nodes'],[10,9])
    def test_truncation_fails(self):
        for data in (b'\x80',b'\x0a\x05ab',b'\x00'):
            with self.assertRaises(ValueError):message(data)
        with self.assertRaises(ValueError):varint(b'\x80'*12,0)

if __name__=='__main__':unittest.main()
