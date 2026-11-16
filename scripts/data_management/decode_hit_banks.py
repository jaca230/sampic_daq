#!/usr/bin/env python3
"""Decode extracted compact SD/SH or optional verbose VD/VH and SB/SR timing payloads."""
import argparse
import json
from pathlib import Path
import struct

SD_HEADER = struct.Struct('<4sHHI')
SD_RECORD = struct.Struct('<BBi6B64hHfd')
SH_HEADER = struct.Struct('<HHI')
SH_RECORD = struct.Struct('<diiQQ')
VERBOSE_SD = struct.Struct('<11i64fi5fdfd')
VERBOSE_SH = struct.Struct('<HiidiiQiiQi')


def framed(payload, header, record, version, magic=None):
    if len(payload) < header.size:
        raise ValueError('truncated bank header')
    values = header.unpack_from(payload)
    if magic is not None:
        if values[0] != magic:
            raise ValueError('unrecognized SD magic')
        values = values[1:]
    actual_version, size, count = values
    if actual_version != version or size != record.size:
        raise ValueError('unsupported bank version or record size')
    if len(payload) != header.size + count * size:
        raise ValueError('bank size disagrees with hit count')
    return record.iter_unpack(payload[header.size:])


def decode_hit_bank(payload, prefix):
    prefix = prefix[:2]
    if prefix == 'SR':
        record = struct.Struct('<HHI5Q2I')
        if len(payload) != record.size:
            raise ValueError('invalid readout tick metadata size')
        version, size, flags, first_tick, ticks, start, end, actual, hits, parents = record.unpack(payload)
        if version != 1 or size != record.size or end < start:
            raise ValueError('invalid readout tick metadata')
        return [dict(flags=flags,first_tick=first_tick,ticks_elapsed=ticks,start_ns=start,
                     end_ns=end,actual_readout_ns=actual,hit_count=hits,parent_count=parents)]
    if prefix == 'SB':
        record = struct.Struct('<HHIqdd')
        if len(payload) != record.size:
            raise ValueError('invalid time-bin metadata size')
        version, size, count, index, start, end = record.unpack(payload)
        if version != 1 or size != record.size or not end > start:
            raise ValueError('invalid time-bin metadata')
        return [dict(bin_index=index, start_ns=start, end_ns=end, hit_count=count)]
    if prefix == 'SD' and payload[:4] == b'SDC1':
        result = []
        for r in framed(payload, SD_HEADER, SD_RECORD, 1, b'SDC1'):
            feb, channel, number, sampic, sampic_channel, size, flags, cell, first = r[:9]
            if size > 64:
                raise ValueError('invalid waveform sample count')
            scale = 0.0001 if flags & 2 else 1.0
            result.append(dict(feb_index=feb, channel=channel, hit_number=number,
                sampic_index=sampic, sampic_channel_index=sampic_channel,
                waveform_size=size, flags=flags, cell_info=cell,
                first_cell_physical_index=first, waveform_units='V' if flags & 2 else 'ADC count',
                waveform=[v * scale for v in r[9:9+size]], waveform_int16=list(r[9:9+size]),
                raw_tot=r[73], calibrated_tot_ns=r[74], first_cell_timestamp_ns=r[75]))
        return result
    if prefix in ('SH', 'VH'):
        if len(payload) < SH_HEADER.size:
            raise ValueError('truncated timing bank')
        version = SH_HEADER.unpack_from(payload)[0]
        if version == 2 and prefix != 'VH':
            names = ['physical_cell0_timestamp_ns','sampic_timestamp_a','sampic_timestamp_b','fpga_timestamp','trigger_position_mask']
            return [dict(zip(names,r)) for r in framed(payload,SH_HEADER,SH_RECORD,2)]
        if version == 1:
            names = ['sampic_data_header','first_trigger_position_cell','trigger_position_cell',
                     'physical_cell0_timestamp_ns','sampic_timestamp_a','sampic_timestamp_b',
                     'fpga_timestamp','adc_counter_latched_at_end_of_conversion',
                     'start_of_adc_ramp','trigger_position_mask','time_physical_index']
            return [dict(zip(names,r)) for r in framed(payload,SH_HEADER,VERBOSE_SH,1)]
        raise ValueError('unsupported timing bank version')
    if prefix in ('VD','SD'):
        if len(payload) % VERBOSE_SD.size:
            raise ValueError('truncated verbose/legacy SD bank')
        result=[]
        for r in VERBOSE_SD.iter_unpack(payload):
            if not 0 <= r[5] <= 64:
                raise ValueError('invalid legacy waveform size')
            result.append(dict(feb_index=r[0],channel=r[1],hit_number=r[2],sampic_index=r[3],
                sampic_channel_index=r[4],waveform_size=r[5],inl_corrected=r[6],adc_corrected=r[7],
                residual_pedestal_corrected=r[8],cell_info=r[9],first_cell_physical_index=r[10],
                waveform=list(r[11:11+r[5]]),waveform_units='V' if r[7] else 'ADC count',
                raw_tot=r[75],calibrated_tot_ns=r[76],amplitude=r[77],baseline=r[78],peak=r[79],
                time_index=r[80],time_instant=r[81],time_amplitude=r[82],first_cell_timestamp_ns=r[83]))
        return result
    raise ValueError('expected SD, SH, VD, VH, SB or SR bank prefix')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix', choices=['SD','SH','VD','VH','SB','SR'])
    parser.add_argument('payload',type=Path)
    args=parser.parse_args()
    print(json.dumps(decode_hit_bank(args.payload.read_bytes(),args.prefix),indent=2))
