"""Reapply the local HCI TX pool exhaustion fix after make getlibs.
Upstream cybt_platform_task_tx_mempool_alloc returned NULL with PRIMASK=1,
which prevented PendSV and corrupted RTOS wait lists on subsequent blocking.
The corrected common exit restores IRQs regardless of allocation result.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/'firmware/mtb_shared/btstack-integration/latest-v6.X/COMPONENT_HCI-UART/common/cybt_platform_task.c'
text=path.read_text()
old='''    if(NULL == p_mem_block)
        return NULL;

    cybt_platform_enable_irq();'''
new='''    /* Local fix: allocation failure must also restore interrupts. */
    cybt_platform_enable_irq();'''
if old in text:path.write_text(text.replace(old,new,1))
elif new not in text:raise SystemExit('Upstream changed: review TX allocation interrupt handling before building')
print('HCI TX allocation interrupt fix verified')
