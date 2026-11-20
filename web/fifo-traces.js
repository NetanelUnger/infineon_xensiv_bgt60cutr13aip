export const rxColors = ['#237763', '#226eaa', '#a35929'];
export const samplesPerChirp = 64;

// Each column represents one simultaneous sample in all three channels.
export function createFifoTraces(container) {
  const svgNS = 'http://www.w3.org/2000/svg';
  const traces = rxColors.map((color, rx) => {
    const row = document.createElement('div');
    row.className = 'fifo-row';
    const label = document.createElement('b');
    label.textContent = 'RX' + (rx + 1);
    label.style.color = color;
    const svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('viewBox', '0 -1 63 2');
    svg.setAttribute('preserveAspectRatio', 'none');
    svg.setAttribute('aria-hidden', 'true');
    const grid = document.createElementNS(svgNS, 'path');
    grid.setAttribute('d', 'M0 0H63 M16 -1V1 M32 -1V1 M48 -1V1');
    grid.setAttribute('class', 'fifo-grid');
    const line = document.createElementNS(svgNS, 'polyline');
    line.setAttribute('stroke', color);
    svg.append(grid, line);
    row.append(label, svg);
    container.append(row);
    return line;
  });
  const ticks = document.createElement('div');
  ticks.className = 'fifo-ticks';
  for (const n of [0, 16, 32, 48, 63]) {
    const tick = document.createElement('span');
    tick.textContent = n;
    tick.style.left = (100 * n / (samplesPerChirp - 1)) + '%';
    ticks.append(tick);
  }
  container.append(ticks);
  return function render(channels, count = samplesPerChirp) {
    traces.forEach((line, rx) => {
      const points = channels[rx].slice(0, count).map((value, n) => n + ',' + (-value).toFixed(4));
      line.setAttribute('points', points.join(' '));
    });
  };
}
