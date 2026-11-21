export function createDashboardTabs(onChange) {
  const tabs = [...document.querySelectorAll('.demo-tabs [role=tab]')];
  function select() {
    const target = document.getElementById(location.hash.slice(1));
    const panel = target?.closest('[role=tabpanel]')?.id || 'ble';
    for (const tab of tabs) {
      const id = tab.getAttribute('aria-controls');
      const active = id === panel;
      document.getElementById(id).hidden = !active;
      tab.setAttribute('aria-selected', String(active));
      tab.tabIndex = active ? 0 : -1;
    }
    document.getElementById('bleConnection').hidden = panel === 'raw';
    onChange(panel);
  }
  function activate(tab) {
    // Keep the mode in browser history without jumping past the tab bar.
    const hash = '#' + tab.getAttribute('aria-controls');
    if (hash !== location.hash) history.pushState(null, '', hash);
    select();
  }
  tabs.forEach((tab, index) => {
    tab.onclick = () => activate(tab);
    tab.onkeydown = event => {
      const next = {ArrowLeft: (index + 1) % tabs.length,
        ArrowRight: (index + tabs.length - 1) % tabs.length, Home: 0, End: tabs.length - 1}[event.key];
      if (next === undefined) return;
      event.preventDefault();
      tabs[next].focus();
      activate(tabs[next]);
    };
  });
  window.addEventListener('hashchange', select);
  window.addEventListener('popstate', select);
  select();
}
