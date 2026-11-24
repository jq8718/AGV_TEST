"""Compare lamp off/on/off via HS CDC without changing sensor calibration."""
import argparse
import json
from pathlib import Path
import statistics
import time

import cv2
import numpy as np
import serial
from cdc_protocol import StreamParser


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--port', required=True)
    args.add_argument('--log-port', required=True)
    args.add_argument('--sensor', choices=['e6', 'ov9281'], required=True)
    args.add_argument('--prefix', help='Artifact prefix, defaults to sensor name')
    args.add_argument('--expect', default='012345')
    opts = args.parse_args()
    prefix = opts.prefix or opts.sensor
    output = Path(__file__).resolve().parents[1] / 'docs'
    log = serial.Serial()
    log.port, log.baudrate, log.timeout = opts.log_port, 115200, 0
    log.dtr, log.rts = True, False
    log.open()
    records, log_bytes = [], bytearray()
    parser = StreamParser()
    try:
        with serial.Serial(opts.port, 921600, timeout=.01) as port:
            port.set_buffer_size(rx_size=2 * 1024 * 1024)
            port.dtr = True
            port.write(b'@TIMINGON\r@GEOMETRYON\r')
            try:
                for phase, light in enumerate((0, 1, 0)):
                    port.write(f'@LIGHT,{light}\r'.encode())
                    started = time.monotonic()
                    request_times = iter((1., 3., 5.))
                    request_at = next(request_times)
                    record = dict(phase=phase, light=light, timings=[], results=[], snapshots=[], errors=[])
                    records.append(record)
                    while time.monotonic() - started < 6.5:
                        elapsed = time.monotonic() - started
                        if elapsed >= request_at:
                            port.write(b'@SNAP\r@BOARD\r')
                            request_at = next(request_times, float('inf'))
                        for kind, value in parser.feed(port.read(65536)):
                            if kind == 'frame':
                                name = f'{prefix}_light_{phase}_{len(record["snapshots"])}.jpg'
                                (output / name).write_bytes(value.jpeg)
                                image = cv2.imdecode(np.frombuffer(value.jpeg, np.uint8), cv2.IMREAD_GRAYSCALE)
                                record['snapshots'].append(dict(file=name, elapsed=elapsed, **value.metadata(),
                                    mean=float(image.mean()), saturated_fraction=float(np.mean(image >= 250))))
                            elif kind == 'result':
                                value['payload'] = value['payload'].decode('utf8', 'replace')
                                value['elapsed'] = elapsed
                                record['results'].append(value)
                            elif kind == 'timing':
                                value['elapsed'] = elapsed
                                record['timings'].append(value)
                            elif kind == 'error':
                                record['errors'].append(value)
                        log_bytes.extend(log.read(log.in_waiting))
                    steady = [t for t in record['timings'] if t['elapsed'] >= 2]
                    ages = [r['age_us'] / 1000 for r in record['results']]
                    record['summary'] = dict(processed=len(record['timings']),
                        successes=sum(t['success'] for t in record['timings']),
                        steady_processed=len(steady), steady_successes=sum(t['success'] for t in steady),
                        matches=sum(r['payload'] == opts.expect for r in record['results']),
                        refined_results=sum(r.get('geometry_quality') == 2 for r in record['results']),
                        results=len(record['results']), snapshots=len(record['snapshots']),
                        median_age_ms=statistics.median(ages) if ages else None,
                        max_age_ms=max(ages) if ages else None)
                    print(json.dumps(dict(phase=phase, light=light, **record['summary'])), flush=True)
            finally:
                port.write(b'@TIMINGOFF\r@GEOMETRYOFF\r@LIGHT,0\r')
                time.sleep(.2)
                port.write(b'@LIGHT,4\r@BOARD\r')
                end = time.monotonic() + .5
                while time.monotonic() < end:
                    port.read(65536)
                    log_bytes.extend(log.read(log.in_waiting))
    finally:
        log.close()
        (output / f'{prefix}_lighting.log').write_bytes(log_bytes)
        (output / f'{prefix}_lighting_validation.json').write_text(
            json.dumps(dict(sensor=opts.sensor, expected=opts.expect, phases=records), indent=2), encoding='utf8')
    return int(any(r['errors'] or len(r['snapshots']) != 3 or not r['summary']['matches'] for r in records))


if __name__ == '__main__':
    raise SystemExit(main())
