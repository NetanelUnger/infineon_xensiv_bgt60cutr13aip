"""Inspect a .radar capture or stream its ADC values to CSV (standard library only)."""
import argparse
import csv
import json
from pathlib import Path
import struct
import sys

HEADER = struct.Struct('<4sHHIIIIIHHHHI')

def records(source):
    if source.read(8) != b'RADC0001':
        raise ValueError('Not a RADC0001 capture')
    while True:
        prefix = source.read(4)
        if not prefix:
            return
        if len(prefix) != 4:
            raise ValueError('Truncated record length; previous records are recoverable')
        size, = struct.unpack('<I', prefix)
        if not 1 <= size <= 1024 * 1024:
            raise ValueError('Invalid record size')
        data = source.read(size)
        if len(data) != size:
            raise ValueError('Truncated record; previous records are recoverable')
        yield data

def inspect(source, output=None):
    meta = footer = previous = None
    count = gaps = fifo = dropped = elapsed = 0
    writer = csv.writer(output) if output else None
    if writer:
        writer.writerow(['session','sequence','software_ms','chirp','sample','rx','adc_u12_4','adc_counts'])
    for data in records(source):
        if footer is not None:
            raise ValueError('Records found after footer')
        if data.startswith(b'RDR1'):
            if not meta or len(data) < HEADER.size:
                raise ValueError('Frame without metadata or truncated header')
            _,version,header,session,seq,ms,errors,queue,chirps,rx,samples,averages,payload = HEADER.unpack_from(data)
            if (version,header,session,chirps,rx,samples,averages,payload,len(data)) != (1,40,meta['session'],17,3,64,16,6528,6568):
                raise ValueError('Frame shape/session mismatch')
            gap = ((seq - previous[0] - 1) & 0xffffffff) if previous else seq
            delta = ((ms - previous[1]) & 0xffffffff) if previous else 0
            if gap > 0x7fffffff or delta > 0x7fffffff or errors < fifo or queue < dropped:
                raise ValueError('Frame order or counters went backwards')
            gaps += gap; elapsed += delta; count += 1
            previous = (seq, ms); fifo = errors; dropped = queue
            if writer:
                for index, (value,) in enumerate(struct.iter_unpack('<H', data[header:])):
                    chirp, sample_rx = divmod(index, samples * rx)
                    sample, antenna = divmod(sample_rx, rx)
                    writer.writerow([session,seq,ms,chirp,sample,antenna,value,value / 16])
        else:
            item = json.loads(data)
            if item.get('type') == 'started' and meta is None and count == 0:
                expected = {'format':'radar-raw-v1','protocol':1,'chirps':17,'rx':3,'samples':64,'averages':16,
                            'encoding':'u12.4','byte_order':'little','order':'chirp_sample_rx','header_bytes':40}
                if any(item.get(k) != v for k, v in expected.items()) or not item.get('registers'):
                    raise ValueError('Unsupported metadata')
                meta = item
            elif item.get('type') == 'footer':
                footer = item
            else:
                raise ValueError('Unexpected JSON record')
    if meta is None:
        raise ValueError('Capture has no metadata')
    server = (footer or {}).get('server') or {}
    complete = bool(footer and footer.get('complete') and server.get('session') == meta['session']
                    and server.get('sent') == count and server.get('offered') == count + server.get('queue_dropped', 0))
    fifo = max(fifo, server.get('fifo_errors', 0)); dropped = max(dropped, server.get('queue_dropped', 0))
    return {'frames':count,'chirps':count*17,'adc_values':count*3264,'sequence_gaps':gaps,
            'queue_dropped':dropped,'fifo_errors':fifo,'elapsed_ms':elapsed,
            'complete':complete,'loss_free':complete and not (gaps or dropped or fifo),
            'reason':(footer or {}).get('reason','missing_footer'),'firmware':meta.get('firmware')}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--csv', type=Path, help='Write every chirp/sample/antenna; CSV can be much larger than the capture')
    args = parser.parse_args()
    if args.csv and args.csv.resolve() == args.capture.resolve():
        parser.error('CSV output must not overwrite the input capture')
    try:
        with args.capture.open('rb') as source:
            if args.csv:
                with args.csv.open('w', newline='', encoding='utf-8') as output:
                    summary = inspect(source, output)
            else:
                summary = inspect(source)
        print(json.dumps(summary, indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        print(f'Capture error: {error}', file=sys.stderr)
        return 1

if __name__ == '__main__':
    sys.exit(main())
