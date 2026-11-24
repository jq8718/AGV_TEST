"""Read sensor CDC timing/results and a CRC-checked same-frame JPEG snapshot.

Run: py -3.12 tools/check_e6.py --seconds 10 [--sensor e6|ov9281] [--port COM68] [--expect 012345]
Requires pyserial, numpy and opencv-python. Does not reset the device.
"""
import argparse
import json
import os
from pathlib import Path
import time

import cv2
import numpy as np
import serial
from serial.tools import list_ports
from cdc_protocol import StreamParser


def main():
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument('--port')
    args.add_argument('--seconds', type=float, default=10)
    args.add_argument('--expect')
    args.add_argument('--prefix', help='Artifact prefix, defaults to sensor name')
    args.add_argument('--sensor', choices=['e6', 'ov9281'], default='e6')
    opts = args.parse_args()
    ports = [p.device for p in list_ports.comports() if (p.vid, p.pid) == (0x303a, 0x8001)]
    if not opts.port and len(ports) != 1:
        args.error(f'Select --port; HS CDC candidates: {ports}')
    if opts.seconds < 1 or opts.seconds > 3600:
        args.error('--seconds must be 1..3600')
    prefix = opts.prefix or opts.sensor
    output = Path(__file__).resolve().parents[1] / 'docs'
    parser = StreamParser()
    timing, results, errors, snapshots = [], [], [], []
    started = time.monotonic()
    request_at = 0
    with serial.Serial(opts.port or ports[0], 921600, timeout=.05) as port:
        if os.name == 'nt':
            # HS CDC bursts can exceed Windows' default serial receive queue.
            port.set_buffer_size(rx_size=2 * 1024 * 1024)
        port.dtr = True
        port.write(b'@TIMINGON\r')
        try:
            while time.monotonic() - started < opts.seconds:
                if not snapshots and time.monotonic() > request_at:
                    port.write(b'@SNAP\r')
                    request_at = time.monotonic() + 1
                for kind, value in parser.feed(port.read(65536)):
                    if kind == 'frame':
                        snapshots.append(value)
                        (output / f'{prefix}_snapshot.jpg').write_bytes(value.jpeg)
                    elif kind == 'timing':
                        timing.append(value)
                    elif kind == 'result':
                        value['payload_hex'] = value['payload'].hex()
                        value['payload'] = value['payload'].decode('utf-8', 'replace')
                        results.append(value)
                    elif kind == 'error':
                        errors.append(value)
        finally:
            port.write(b'@TIMINGOFF\r@DEBUGOFF\r')
    report = dict(seconds=time.monotonic()-started, processed=len(timing),
                  successes=sum(t['success'] for t in timing), results=results,
                  protocol_errors=errors, snapshots=len(snapshots))
    if len(timing) > 1:
        span = (timing[-1]['capture_us'] - timing[0]['capture_us']) / 1e6
        if span > 0:
            report['processed_fps'] = (len(timing)-1)/span
            report['sensor_sequence_fps'] = (timing[-1]['sequence']-timing[0]['sequence'])/span
    if snapshots:
        snap = snapshots[-1]
        report['snapshot'] = snap.metadata()
        image = cv2.imdecode(np.frombuffer(snap.jpeg, np.uint8), cv2.IMREAD_GRAYSCALE)
        if image is None:
            errors.append('JPEG decode failed')
        else:
            report['image'] = dict(shape=list(image.shape), mean=float(image.mean()),
                                   minimum=int(image.min()), maximum=int(image.max()))
    if opts.expect:
        report['expected'] = opts.expect
        report['matches'] = sum(r['payload'] == opts.expect for r in results)
    report['sensor_profile'] = opts.sensor
    report_path = output / f'{prefix}_live_validation.json'
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    summary = {key: value for key, value in report.items() if key != 'results'}
    summary['results_received'] = len(results)
    summary['report_file'] = str(report_path)
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return int(bool(errors) or not snapshots or (opts.expect is not None and not report['matches']))


if __name__ == '__main__':
    raise SystemExit(main())
