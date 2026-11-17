"""Run only with radar on a safe bench, induction/charging equipment powered off.
Requires research firmware 0.2.0. Tests real BLE capture, repeatable paging,
reconnect download, and retained 5-second data. Does not control external power.
"""
import asyncio, json, time
from pathlib import Path
from bleak import BleakClient, BleakScanner
from probe_ble import Probe, SERVICE, TX

async def main():
    Path('logs').mkdir(exist_ok=True)
    result={'passed':False,'records':[]}
    p=Probe()
    try:
        device=await BleakScanner.find_device_by_filter(lambda d,a:SERVICE in a.service_uuids,timeout=15)
        if not device: raise RuntimeError('Radar-CSK not found')
        async with BleakClient(device,timeout=20) as c:
            await c.start_notify(TX,p.notification)
            info=next(x for x in await p.command(c,1) if 'version' in x)
            assert info['version']=='0.2.0',info['version']
            meta=next(x['research'] for x in await p.command(c,20,{'duration_ms':5000}) if 'research' in x)
            cid=meta['id']; deadline=time.monotonic()+15
            while meta['active']:
                assert time.monotonic()<deadline,'Capture stuck'
                await asyncio.sleep(.5)
                meta=next(x['research'] for x in await p.command(c,21) if 'research' in x)
            assert meta['id']==cid and meta['reason']=='duration' and meta['count']>10,meta
            first=next(x['research_page'] for x in await p.command(c,22,{'id':cid,'offset':0}) if 'research_page' in x)
            again=next(x['research_page'] for x in await p.command(c,22,{'id':cid,'offset':0}) if 'research_page' in x)
            assert first==again,'Readback not stable'
        # Recording is retained through BLE disconnect, until reboot/new capture.
        p.pending=None
        async with BleakClient(device,timeout=20) as c:
            await c.start_notify(TX,p.notification)
            rows=[]
            while len(rows)<meta['count']:
                page=next(x['research_page'] for x in await p.command(c,22,{'id':cid,'offset':len(rows)}) if 'research_page' in x)
                assert page['id']==cid and page['offset']==len(rows) and 1<=len(page['rows'])<=2
                rows.extend(page['rows'])
            assert len(rows)==meta['count']
            assert all(len(r['adc'])==192 and all(isinstance(v,int) and 0<=v<=65535 for v in r['adc']) for r in rows)
            assert all(a['ms']<b['ms'] for a,b in zip(rows,rows[1:]))
            assert not p.errors,p.errors
            result.update(passed=True,meta=meta,rows=rows,firmware=info['version'],reconnect_download=True,repeat_page_identical=True)
            Path('logs/research-capture.json').write_text(json.dumps({'format':'radar-research-v1','downloadedAt':time.strftime('%Y-%m-%dT%H:%M:%S'), 'firmware':info['version'],'meta':meta,'rows':rows}),encoding='utf-8')
    finally:
        result['errors']=p.errors
        Path('logs/research-probe.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        print(json.dumps({k:v for k,v in result.items() if k not in ('rows','records')},indent=2))

if __name__=='__main__': asyncio.run(main())
