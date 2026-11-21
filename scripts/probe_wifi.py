"""Hardware smoke test: unpaired BLE commands, Wi-Fi initialization, failed join,
and BLE responsiveness after the failure. Uses an intentionally absent test SSID.
Does not prove a successful LAN connection or raw WebSocket throughput.
"""
import asyncio
import json
from pathlib import Path
import time
from bleak import BleakClient, BleakScanner
from probe_ble import Probe, SERVICE, TX

async def main():
    probe = Probe()
    device = await BleakScanner.find_device_by_filter(lambda d,a: SERVICE in a.service_uuids, timeout=15)
    if not device:
        raise RuntimeError('Radar-CSK not found')
    result = {'passed':False,'states':[]}
    try:
        async with BleakClient(device, timeout=20) as client:
            await client.start_notify(TX, probe.notification)
            info = next(x for x in await probe.command(client,1) if 'version' in x)
            assert info['version'] in ('0.3.0','0.3.1') and info['capabilities']['raw_websocket'] == 1
            initial = next(x['wifi'] for x in await probe.command(client,31) if 'wifi' in x)
            assert initial['state'] in ('idle','disconnected','error'), 'Existing Wi-Fi connection: skip destructive smoke test'
            result['states'].append(initial)
            await probe.command(client,30,{'ssid':'Radar-No-AP-Test-5e49-20261118','password':'demo-only-test'})
            deadline=time.monotonic()+100
            while True:
                await asyncio.sleep(2)
                status=next(x['wifi'] for x in await probe.command(client,31) if 'wifi' in x)
                result['states'].append(status)
                print(json.dumps(status),flush=True)
                if not status['busy']:
                    break
                assert time.monotonic()<deadline,'Wi-Fi join did not time out'
            assert status['state']=='error' and status['error_code'] != 0
            assert status['initialized'] and not status['driver_error'], 'Radio initialization failed before scan/join'
            await probe.command(client,32)
            await asyncio.sleep(1)
            final=next(x['wifi'] for x in await probe.command(client,31) if 'wifi' in x)
            assert final['state']=='disconnected' and not final['busy']
            settings=next(x['settings'] for x in await probe.command(client,7) if 'settings' in x)
            assert settings and not probe.errors, probe.errors
            result.update(passed=True,firmware=info['version'],final=final,ble_frames=sum('tracks' in x for x in probe.records))
    finally:
        result['errors']=probe.errors
        Path('logs').mkdir(exist_ok=True)
        Path('logs/wifi-init-probe.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps(result,indent=2),flush=True)

if __name__ == '__main__':
    asyncio.run(main())
