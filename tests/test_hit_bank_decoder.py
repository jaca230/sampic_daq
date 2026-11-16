import importlib.util
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('decoder', ROOT/'scripts/data_management/decode_hit_banks.py')
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)
FIXTURES = Path(sys.argv.pop(1)) if len(sys.argv)>1 else ROOT/'build/bank_test_fixtures'

class DecoderTests(unittest.TestCase):
    def test_cpp_payloads(self):
        data = {p:decoder.decode_hit_bank((FIXTURES/f'{p}.bin').read_bytes(),p) for p in ('SD','SH','VD','VH')}
        for prefix in data:
            self.assertEqual(len(data[prefix]),1)
        sd, vd = data['SD'][0], data['VD'][0]
        self.assertEqual(sd['waveform_int16'],[1234,-456])
        for key in ('feb_index','channel','hit_number','sampic_index','sampic_channel_index',
                    'first_cell_timestamp_ns','raw_tot','calibrated_tot_ns'):
            self.assertEqual(sd[key],vd[key])
        for compact, verbose in zip(sd['waveform'],vd['waveform']):
            self.assertLess(abs(compact-verbose),0.00005)
        for key,value in data['SH'][0].items():
            self.assertEqual(value,data['VH'][0][key])
        self.assertEqual(data['SH'][0]['trigger_position_mask'],(1<<63)|1)
        self.assertEqual(decoder.decode_hit_bank((FIXTURES/'VH.bin').read_bytes(),'SH'),data['VH'])
        self.assertEqual(decoder.decode_hit_bank((FIXTURES/'VD.bin').read_bytes(),'SD'),data['VD'])

    def test_bad_frames_rejected(self):
        original=(FIXTURES/'SD.bin').read_bytes()
        for payload in (original[:-1],original+b'\x00',b'SDC1',
                        original[:4]+struct.pack('<H',99)+original[6:],
                        original[:8]+struct.pack('<I',2)+original[12:]):
            with self.assertRaises(ValueError):decoder.decode_hit_bank(payload,'SD')
        for payload in (b'',(FIXTURES/'SH.bin').read_bytes()[:-1]):
            with self.assertRaises(ValueError):decoder.decode_hit_bank(payload,'SH')
        with self.assertRaises(ValueError):decoder.decode_hit_bank(b'\x00','VD')

    def test_time_bin_metadata(self):
        payload=struct.pack('<HHIqdd',1,32,0,3,30000,40000)
        self.assertEqual(decoder.decode_hit_bank(payload,'SB'),
            [dict(bin_index=3,start_ns=30000,end_ns=40000,hit_count=0)])
        with self.assertRaises(ValueError):decoder.decode_hit_bank(payload[:-1],'SB')

    def test_readout_tick_metadata(self):
        payload=struct.pack('<HHI5Q2I',1,56,3,2,3,20000,50000,55000,17,2)
        result=decoder.decode_hit_bank(payload,'SR')[0]
        self.assertEqual(result['ticks_elapsed'],3)
        self.assertEqual(result['actual_readout_ns'],55000)
        self.assertEqual(result['hit_count'],17)
        with self.assertRaises(ValueError):decoder.decode_hit_bank(payload[:-1],'SR')

    def test_zero_hits(self):
        self.assertEqual(decoder.decode_hit_bank(struct.pack('<4sHHI',b'SDC1',1,154,0),'SD'),[])
        self.assertEqual(decoder.decode_hit_bank(struct.pack('<HHI',2,32,0),'SH'),[])

if __name__=='__main__':unittest.main()
