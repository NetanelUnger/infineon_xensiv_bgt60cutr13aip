"""Provision over unpaired BLE and wait for an IP. Credentials stay in RAM.
Use --stdin-credentials to read one JSON object from stdin; otherwise prompt.
"""
import argparse
import asyncio
import getpass
import json
from pathlib import Path
import sys
import time
from bleak import BleakClient, BleakScanner
from probe_ble import Probe, SERVICE, TX

async def main(credentials):
    device=await BleakScanner.find_device_by_filter(lambda d,a:SERVICE in a.service_uuids,timeout=15)
    if not device:
        raise RuntimeError('Radar-CSK not found; disconnect its browser BLE session before running the probe')
    p=Probe(); status=None; previous=None
    async with BleakClient(device,timeout=20) as client:
        await client.start_notify(TX,p.notification)
        info=next(x for x in await p.command(client,1) if 'version' in x)
        await p.command(client,30,credentials)
        credentials.clear()
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            await asyncio.sleep(1)
            status=next(x['wifi'] for x in await p.command(client,31) if 'wifi' in x)
            if status!=previous:
                print(json.dumps(status),flush=True);previous=status
            if not status['busy']:
                break
        result={'firmware':info['version'],'status':status,'fragment_errors':p.errors,
                'ble_frames':sum('tracks' in x for x in p.records)}
        Path('logs').mkdir(exist_ok=True)
        Path('logs/wifi-join-probe.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        assert status and status['state']=='connected' and not status['busy'], 'Wi-Fi join failed; see the status above'
        assert not p.errors,p.errors
        print('Wi-Fi joined; BLE notifications remained active.',flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stdin-credentials',action='store_true')
    args=parser.parse_args()
    credentials=json.load(sys.stdin) if args.stdin_credentials else {'ssid':input('SSID: '),'password':getpass.getpass('Wi-Fi password: ')}
    asyncio.run(main(credentials))
