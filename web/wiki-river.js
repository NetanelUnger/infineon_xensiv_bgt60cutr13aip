(() => {
  const get = id => document.getElementById(id);
  const pen = get('riverPlot').getContext('2d');
  const play = get('riverPlay');
  const target = get('riverTarget');
  const colors = ['#237763', '#226eaa', '#a35929'];
  const tau = 2 * Math.PI;
  let progress = 0, timer, spectrum = [];
  function sample(row, n, rx) {
    const noise = Math.sin((row * 192 + n * 3 + rx + 1) * 78.233) * 43758.5453;
    return (noise - Math.floor(noise) - 0.5) * 0.2 +
      (Number(target.value) ? 0.8 * Math.sin(tau * target.value * n / 64 + rx * 0.45) : 0);
  }
  function label(text, x, y) {
    pen.fillStyle = '#183d38';
    pen.textAlign = 'left';
    pen.font = '16px sans-serif';
    pen.fillText(text, x, y);
  }
  function path(x, y, endX, endY, fraction, color) {
    pen.strokeStyle = color;
    pen.beginPath();
    pen.moveTo(x, y);
    pen.lineTo(x + (endX - x) * fraction, y + (endY - y) * fraction);
    pen.stroke();
  }
  function draw() {
    const rows = Math.floor(progress / 100);
    const phase = progress % 100;
    const samples = Math.max(0, Math.min(64, phase - 35));
    const objectX = target.value === '9' ? 550 : 370;
    pen.fillStyle = '#faf9f3';
    pen.fillRect(0, 0, 650, 445);
    pen.lineWidth = 3;
    label('TX', 12, 27);
    path(55, 22, objectX, 45, Math.min(1, phase / 18), '#b27d18');
    if (+target.value) {
      pen.fillStyle = '#445452';
      pen.fillRect(objectX, 24, 9, 48);
      label('לוח', objectX - 4, 95);
    }
    colors.forEach((color, rx) => {
      const y = 54 + rx * 19;
      label('RX' + (rx + 1), 10, y);
      if (+target.value) path(objectX, 45, 55, y - 4, Math.min(1, Math.max(0, (phase - 18) / 17)), color);
    });
    label('FIFO  ·  ADC', 18, 127);
    for (let row = 0; row < Math.min(17, rows + 1); row++) {
      for (let n = 0; n < (row < rows ? 64 : samples); n++) {
        for (let rx = 0; rx < 3; rx++) {
          const light = 49 + 38 * sample(row, n, rx);
          pen.fillStyle = 'hsl(166 42% ' + light + '%)';
          pen.fillRect(36 + 9*n + 3*rx, 136 + 9*row, 3, 8);
        }
      }
    }
    label('1', 10, 145);
    label('17', 8, 289);
    label('RX1  ·  עוצמה לפי תא טווח', 20, 326);
    if (rows) {
      spectrum = Array.from({length:32}, (_, k) => {
        let real = 0, imag = 0;
        for (let n = 0; n < 64; n++) {
          const v = sample(rows-1, n, 0);
          real += v * Math.cos(tau*k*n/64);
          imag += v * Math.sin(tau*k*n/64);
        }
        return Math.hypot(real, imag)/32;
      });
      spectrum.forEach((v,k) => {
        pen.fillStyle = '#257762';
        pen.fillRect(36+k*18, 411-v*88, 13, v*88);
      });
    }
    path(36,412,612,412,1,'#849d95');
    for (const k of [0,4,9,16,24,31]) label(String(k),36+18*k,435);
    get('riverRead').textContent = rows + ' מתוך 17 צ׳ירפים הושלמו · ' +
      (rows*192 + samples*3) + ' ערכים נכתבו במסגרת' +
      (rows && +target.value ? ' · שיא בתא ' + spectrum.indexOf(Math.max(...spectrum)) : '');
  }
  function stop() {
    clearInterval(timer);
    timer = undefined;
    play.textContent = 'הפעלה';
  }
  play.onclick = () => {
    if (timer) return stop();
    if (progress === 1700) progress = 0;
    play.textContent = 'השהיה';
    timer = setInterval(() => {
      progress += 1;
      draw();
      if (progress === 1700) stop();
    }, 25);
  };
  get('riverStep').onclick = () => {
    stop();
    progress = progress === 1700 ? 100 : (Math.floor(progress / 100) + 1) * 100;
    draw();
  };
  target.onchange = () => { stop(); progress = 0; draw(); };
  draw();
})();
