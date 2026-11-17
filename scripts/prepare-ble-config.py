"""Apply Radar-CSK settings to the checked-in Bluetooth Configurator input.

GeneratedSource is already committed. Regenerate it with Bluetooth Configurator
only when intentionally changing the design; no vendor reference clone is needed.
"""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
design = root / 'firmware/radar-app/design.cybt'
text = design.read_text()
text = text.replace('Find Me Target', 'Radar-CSK')
text = text.replace('id="MtuSize" value="23"', 'id="MtuSize" value="247"')
text = text.replace('id="HostLowAdvTimeoutEnabled" value="true"', 'id="HostLowAdvTimeoutEnabled" value="false"')
text = text.replace('id="HostHighAdvTimeoutEnabled" value="true"', 'id="HostHighAdvTimeoutEnabled" value="false"')
design.write_text(text)
