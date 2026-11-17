"""Hardware integration test: discovery, fragmented notifications, commands, reconnect.
--roundtrip temporarily changes tilt by 0.5 degrees and restores the prior settings.
"""
import argparse, asyncio, binascii, json, struct, time
from copy import deepcopy
from pathlib import Path
from bleak import BleakClient, BleakScanner
SERVICE = '83f09d00-62e6-4b88-9c74-2d519c840cb1'
RX = '83f09d01-62e6-4b88-9c74-2d519c840cb1'
TX = '83f09d02-62e6-4b88-9c74-2d519c840cb1'

class Probe:
    def __init__(self):
        self.records=[]; self.pending=None; self.errors=[]
    def notification(self, _, data):
        if len(data)<7: self.errors.append('short packet'); return
        mid,off,total=struct.unpack_from('<HHH',data)
        if off==0: self.pending=(mid,total,bytearray())
        if not self.pending: return
        old,size,buf=self.pending
        if old!=mid or total!=size or off!=len(buf) or total>12288 or off+len(data)-6>total:
            self.errors.append('fragment mismatch'); self.pending=None; return
        buf.extend(data[6:])
        if len(buf)==total:
            try: self.records.append(json.loads(buf))
            except (ValueError,UnicodeDecodeError): self.errors.append('invalid JSON')
            self.pending=None
    async def command(self, client, cid, payload=None):
        raw=b'' if payload is None else json.dumps(payload,separators=(',',':')).encode()
        data=struct.pack('<BBH',0xd9,cid,len(raw))+raw
        data+=struct.pack('<H',binascii.crc_hqx(data,0xffff))
        start=len(self.records)
        for offset in range(0,len(data),20):
            await client.write_gatt_char(RX,data[offset:offset+20],response=True)
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            new=self.records[start:]
            ack=next((x['cmd'] for x in new if x.get('cmd',{}).get('id')==cid),None)
            if ack is not None:
                assert ack['state'],f'command {cid} rejected'
                assert not any('warning' in x or 'error' in x for x in new),new
                return new
            await asyncio.sleep(.05)
        raise TimeoutError(f'Command {cid}: no acknowledgment; records={len(self.records)}')

async def main(args):
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    device=await BleakScanner.find_device_by_filter(lambda d,a: SERVICE in a.service_uuids,timeout=15)
    if not device: raise RuntimeError('Radar-CSK not discovered')
    print('Discovered',device,flush=True)
    p=Probe(); result={'device':device.address,'sessions':[]}
    try:
        for session in range(2):
            p.pending=None; start=len(p.records)
            async with BleakClient(device,timeout=20) as c:
                await c.start_notify(TX,p.notification)
                info=await p.command(c,1)
                settings=next(x['settings'] for x in await p.command(c,7) if 'settings' in x)
                print('Connected, MTU',c.mtu_size,'firmware',next(x['name'] for x in info if 'name' in x),flush=True)
                if session==0 and args.roundtrip:
                    updated=deepcopy(settings)
                    old=updated['sensor_mounting']['tilt_angle_deg']
                    updated['sensor_mounting']['tilt_angle_deg']=old+.5 if old<44 else old-.5
                    try:
                        await p.command(c,3,updated)
                        actual=next(x['settings'] for x in await p.command(c,7) if 'settings' in x)
                        assert actual==updated,(actual,updated)
                        result['configuration_roundtrip']=True
                    finally:
                        await p.command(c,3,settings)
                        restored=next(x['settings'] for x in await p.command(c,7) if 'settings' in x)
                        assert restored==settings
                        result['configuration_restored']=True
                await asyncio.sleep(args.seconds)
                frames=[x for x in p.records[start:] if 'tracks' in x]
                assert frames,'No radar frames'
                result['sessions'].append({'mtu':c.mtu_size,'frames':len(frames),'frames_dropped_max':max(x.get('frames_dropped',0) for x in frames)})
            await asyncio.sleep(1)
        assert not p.errors,p.errors
        result['passed']=True
    finally:
        result['errors']=p.errors; result['records']=p.records
        Path(args.output).write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps({k:v for k,v in result.items() if k!='records'},indent=2),flush=True)

if __name__=='__main__':
    a=argparse.ArgumentParser(); a.add_argument('--seconds',type=float,default=8)
    a.add_argument('--roundtrip',action='store_true'); a.add_argument('--output',default='logs/ble-probe.json')
    asyncio.run(main(a.parse_args()))
