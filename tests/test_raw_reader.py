import csv
import importlib.util
import io
import json
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('raw_reader', Path(__file__).resolve().parents[1]/'scripts/read-raw-capture.py')
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
META = dict(type='started',format='radar-raw-v1',protocol=1,session=12,chirps=17,rx=3,samples=64,averages=16,
            encoding='u12.4',byte_order='little',order='chirp_sample_rx',header_bytes=40,registers=[[1,2]])

def record(value):
    data = value if isinstance(value, bytes) else json.dumps(value).encode()
    return struct.pack('<I', len(data)) + data

def frame(seq=0, ms=1000):
    return reader.HEADER.pack(b'RDR1',1,40,12,seq,ms,0,0,17,3,64,16,6528) + struct.pack('<3264H', *range(3264))

class ReaderTests(unittest.TestCase):
    def test_complete_csv_contains_every_antenna_and_last_chirp(self):
        footer=dict(type='footer',complete=True,reason='requested',server=dict(session=12,sent=1,offered=1,queue_dropped=0,fifo_errors=0))
        capture=b'RADC0001'+record(META)+record(frame())+record(footer)
        output=io.StringIO();result=reader.inspect(io.BytesIO(capture),output)
        self.assertTrue(result['loss_free']); self.assertEqual(result['adc_values'],3264)
        rows=list(csv.reader(io.StringIO(output.getvalue())))
        self.assertEqual(len(rows),3265)
        self.assertEqual(rows[-1],['12','0','1000','16','63','2','3263','203.9375'])
    def test_missing_footer_remains_readable_but_partial(self):
        result=reader.inspect(io.BytesIO(b'RADC0001'+record(META)+record(frame())))
        self.assertFalse(result['complete']); self.assertEqual(result['reason'],'missing_footer')
    def test_truncated_record_rejected(self):
        with self.assertRaisesRegex(ValueError,'Truncated record'):
            reader.inspect(io.BytesIO(b'RADC0001'+record(META)+record(frame())[:-1]))
    def test_duplicate_or_wrong_metadata_rejected(self):
        with self.assertRaisesRegex(ValueError,'backwards'):
            reader.inspect(io.BytesIO(b'RADC0001'+record(META)+record(frame())+record(frame())))
        with self.assertRaisesRegex(ValueError,'Unsupported metadata'):
            reader.inspect(io.BytesIO(b'RADC0001'+record({**META,'chirps':1})))

if __name__ == '__main__':
    unittest.main()
