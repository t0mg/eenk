/**
 * flasher.js — eenk Web Serial Flasher
 *
 * Portable zero-dependency module. Works in:
 *   - Chrome / Edge browser (GitHub Pages)
 *   - Electron (eenky Device Manager iframe)
 *
 * Assumes esptool-js UMD bundle is loaded before this script.
 * Global: window.ESPLoader, window.Transport (from esptool.bundle.js)
 *
 * Manifest format (fetched per release from GitHub):
 *   manifest.json → { targets: { X4: { factory: { chip, binaries: [{offset, url}] }, ... } } }
 */

'use strict';

/* ── Constants ─────────────────────────────────────────────────── */
const GITHUB_REPO = 't0mg/eenk';
const RELEASES_API = `https://api.github.com/repos/${GITHUB_REPO}/releases`;
const FLASH_BAUD = 921600;

/** CrossPoint / Xteink stock firmware download pages per target */
const CROSSPOINT_LINKS = {
  X3: 'https://www.crosspoint.fr/firmware',
  X4: 'https://www.crosspoint.fr/firmware',
  X4Pro: 'https://www.crosspoint.fr/firmware',
};

/** Auto-download URL patterns (attempt before requesting manual upload) */
const CROSSPOINT_AUTO_URLS = {
  X3: null,  // Fill in if a direct-download URL becomes available
  X4: null,
  X4Pro: null,
};

/* ── Context Detection ──────────────────────────────────────────── */
const IS_ELECTRON = (
  (typeof window !== 'undefined' && typeof window.electronBridge !== 'undefined') ||
  (typeof navigator !== 'undefined' && /Electron/.test(navigator.userAgent))
);

const IS_IFRAME = typeof window !== 'undefined' && window.self !== window.top;

if (IS_ELECTRON) {
  document.body.classList.add('electron-embed');
}
if (IS_IFRAME) {
  document.body.classList.add('iframe-embed');
}

/* ── State ──────────────────────────────────────────────────────── */
const state = {
  step: 1,
  target: null,   // 'X3' | 'X4' | 'X4Pro'
  mode: null,   // 'factory' | 'update' | 'crosspoint'
  releaseTag: null,
  manifest: null,
  port: null,
  transport: null,
  loader: null,
  flashing: false,
  showAllReleases: false,
  stableReleases: [],
  allReleases: [],
  crosspointBinaryData: null,  // ArrayBuffer from file picker or auto-download
};

/* ── DOM references ─────────────────────────────────────────────── */
const $id = id => document.getElementById(id);

// Step panels & sidebar
const stepPanels = [1, 2, 3, 4].map(n => $id(`step-${n}`));
const sidebarSteps = [1, 2, 3, 4].map(n => $id(`sidebar-step-${n}`));

// Step 1
const unlockConfirm = $id('unlock-confirm');
const step1Next = $id('step1-next');

// Step 2
const targetCards = document.querySelectorAll('[data-target]');
const step2Back = $id('step2-back');
const step2Next = $id('step2-next');

// Step 3
const modeCards = document.querySelectorAll('[data-mode]');
const step3Back = $id('step3-back');
const step3Next = $id('step3-next');

// Step 4
const versionSelect = $id('version-select');
const showAllBtn = $id('show-all-versions');
const versionError = $id('version-error');
const versionSection = $id('version-section');
const crosspointSection = $id('crosspoint-section');
const connectBtn = $id('connect-btn');
const disconnectBtn = $id('disconnect-btn');
const portBadge = $id('port-badge');
const flashBtn = $id('flash-btn');
const step4Back = $id('step4-back');
const progressSection = $id('progress-section');
const progressBar = $id('progress-bar');
const progressLabel = $id('progress-label-text');
const progressPct = $id('progress-pct');
const flashLog = $id('flash-log');
const flashDone = $id('flash-done');
const flashAgainBtn = $id('flash-again-btn');
const summaryTarget = $id('summary-target');
const summaryMode = $id('summary-mode');

// CrossPoint
const crosspointAutoBtn = $id('crosspoint-auto-btn');
const crosspointFileBtn = $id('crosspoint-file-btn');
const crosspointFileInput = $id('crosspoint-file');
const crosspointFileName = $id('crosspoint-file-name');
const crosspointDownloadLink = $id('crosspoint-download-link');
const crosspointEraseBtn = $id('crosspoint-erase-btn');
const fallback3Model = $id('fallback3-model');

/* ── Navigation ─────────────────────────────────────────────────── */
function goToStep(n) {
  state.step = n;

  stepPanels.forEach((p, i) => {
    p.classList.toggle('active', i + 1 === n);
  });

  sidebarSteps.forEach((s, i) => {
    const stepN = i + 1;
    s.classList.remove('active', 'done');
    if (stepN === n) s.classList.add('active');
    else if (stepN < n) s.classList.add('done');
  });

  if (n === 4) renderStep4();
}

/* ── Step 1: Safety Gate ────────────────────────────────────────── */
unlockConfirm.addEventListener('change', () => {
  step1Next.disabled = !unlockConfirm.checked;
});

step1Next.addEventListener('click', () => goToStep(2));

/* ── Step 2: Target ─────────────────────────────────────────────── */
targetCards.forEach(card => {
  card.addEventListener('click', () => {
    targetCards.forEach(c => {
      c.classList.remove('selected');
      c.setAttribute('aria-checked', 'false');
    });
    card.classList.add('selected');
    card.setAttribute('aria-checked', 'true');
    state.target = card.dataset.target;
    step2Next.disabled = false;
  });
});

step2Back.addEventListener('click', () => goToStep(1));
step2Next.addEventListener('click', () => goToStep(3));

/* ── Step 3: Mode ───────────────────────────────────────────────── */
modeCards.forEach(card => {
  card.addEventListener('click', () => {
    modeCards.forEach(c => {
      c.classList.remove('selected');
      c.setAttribute('aria-checked', 'false');
    });
    card.classList.add('selected');
    card.setAttribute('aria-checked', 'true');
    state.mode = card.dataset.mode;
    step3Next.disabled = false;
  });
});

step3Back.addEventListener('click', () => goToStep(2));
step3Next.addEventListener('click', () => goToStep(4));

/* ── Step 4: Render ─────────────────────────────────────────────── */
function renderStep4() {
  summaryTarget.textContent = state.target || '—';
  summaryMode.textContent = { factory: 'Factory Flash', update: 'Update', crosspoint: 'CrossPoint / Stock Restore' }[state.mode] || '—';

  const isCrosspoint = state.mode === 'crosspoint';
  versionSection.style.display = isCrosspoint ? 'none' : '';
  crosspointSection.style.display = isCrosspoint ? '' : 'none';

  if (isCrosspoint) {
    renderCrosspointUI();
  } else {
    loadReleases();
  }

  updateFlashButton();
}

/* ── Releases API ───────────────────────────────────────────────── */
async function loadReleases() {
  if (state.stableReleases.length > 0) {
    populateVersionDropdown();
    return;
  }
  versionSelect.innerHTML = '<option value="">Loading releases…</option>';
  try {
    const res = await fetch(RELEASES_API);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const all = await res.json();
    state.allReleases = all;
    state.stableReleases = all.filter(r => !r.prerelease && !r.draft);
    populateVersionDropdown();
  } catch (err) {
    versionError.style.display = '';
    versionSelect.innerHTML = '<option value="">Failed to load</option>';
    log(`${mi('warning')} Could not fetch releases: ${err.message}`, 'error');
  }
}

function populateVersionDropdown() {
  const releases = state.showAllReleases ? state.allReleases : state.stableReleases;
  versionError.style.display = 'none';

  if (releases.length === 0) {
    versionSelect.innerHTML = '<option value="">No releases found</option>';
    return;
  }

  versionSelect.innerHTML = releases.map((r, i) => {
    const label = r.prerelease ? `${r.tag_name} (pre-release)` : r.tag_name;
    return `<option value="${r.tag_name}" ${i === 0 ? 'selected' : ''}>${label}</option>`;
  }).join('');

  state.releaseTag = versionSelect.value;
  updateFlashButton();
}

versionSelect.addEventListener('change', () => {
  state.releaseTag = versionSelect.value;
  state.manifest = null;  // reset cached manifest on version change
  updateFlashButton();
});

showAllBtn.addEventListener('click', () => {
  state.showAllReleases = !state.showAllReleases;
  showAllBtn.setAttribute('aria-pressed', state.showAllReleases.toString());
  showAllBtn.textContent = state.showAllReleases ? 'Stable only' : 'Show all';
  populateVersionDropdown();
});

/* ── CrossPoint UI ──────────────────────────────────────────────── */
function renderCrosspointUI() {
  crosspointDownloadLink.href = CROSSPOINT_LINKS[state.target] || '#';
  crosspointDownloadLink.textContent = 'CrossPoint / Xteink firmware page';
  if (fallback3Model) fallback3Model.textContent = state.target;

  const hasAutoUrl = !!CROSSPOINT_AUTO_URLS[state.target];
  if (!hasAutoUrl) {
    // Skip auto-download step — activate step 2 immediately
    activateFallback(2);
  } else {
    activateFallback(1);
  }
  crosspointEraseBtn.disabled = !state.port;
}

function activateFallback(n) {
  [1, 2, 3, 4].forEach(i => {
    const el = $id(`fallback-${i}`);
    if (!el) return;
    el.classList.remove('active-fallback', 'completed');
    if (i < n) el.classList.add('completed');
    if (i === n) el.classList.add('active-fallback');
  });
}

crosspointAutoBtn?.addEventListener('click', async () => {
  const url = CROSSPOINT_AUTO_URLS[state.target];
  if (!url) { activateFallback(2); return; }
  log(`${mi('download')} Attempting auto-download of stock firmware…`, 'info');
  try {
    const res = await fetch(url);
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    state.crosspointBinaryData = await res.arrayBuffer();
    crosspointFileName.textContent = 'Downloaded automatically';
    activateFallback(5); // all done — enable flash
    updateFlashButton();
    log(`${mi('check_circle')} Stock firmware downloaded.`, 'info');
  } catch (err) {
    log(`${mi('error')} Auto-download failed: ${err.message}. Try manual download.`, 'error');
    activateFallback(2);
  }
});

crosspointFileBtn?.addEventListener('click', () => crosspointFileInput.click());

crosspointFileInput?.addEventListener('change', async () => {
  const file = crosspointFileInput.files[0];
  if (!file) return;
  try {
    state.crosspointBinaryData = await file.arrayBuffer();
    // Validate: check ESP32 magic byte (0xE9)
    const header = new Uint8Array(state.crosspointBinaryData, 0, 1);
    if (header[0] !== 0xE9) {
      throw new Error('File does not appear to be a valid ESP32 binary (bad magic byte).');
    }
    crosspointFileName.textContent = file.name;
    log(`${mi('check_circle')} Loaded: ${file.name} (${(file.size / 1024).toFixed(0)} KB)`, 'info');
    updateFlashButton();
  } catch (err) {
    log(`${mi('error')} File error: ${err.message}`, 'error');
    state.crosspointBinaryData = null;
    crosspointFileName.textContent = '';
  }
});

crosspointEraseBtn?.addEventListener('click', async () => {
  if (!state.port) return;
  await initLoader();
  log(`${mi('warning')} Erasing flash…`, 'info');
  await state.loader.erase_flash();
  log(`${mi('check_circle')} Erased. Opening online flasher…`, 'info');
  const url = CROSSPOINT_LINKS[state.target] || 'https://www.crosspoint.fr/firmware';
  if (IS_ELECTRON && window.electronBridge?.openExternal) {
    window.electronBridge.openExternal(url);
  } else {
    window.open(url, '_blank', 'noopener noreferrer');
  }
});

/* ── Connect / Disconnect ───────────────────────────────────────── */
connectBtn.addEventListener('click', async () => {
  if (!('serial' in navigator)) {
    alert('Web Serial API is not available.\nPlease use Chrome or Edge (version 89+), or the eenky app.');
    return;
  }
  try {
    state.port = await navigator.serial.requestPort();
    state.transport = new Transport(state.port, true);
    portBadge.textContent = `Connected (${getPortName(state.port)})`;
    portBadge.classList.add('connected');
    connectBtn.style.display = 'none';
    disconnectBtn.style.display = '';
    if (state.mode === 'crosspoint') crosspointEraseBtn.disabled = false;
    log(`${mi('usb')} Serial port selected.`, 'info');
    updateFlashButton();
  } catch (err) {
    if (err.name !== 'NotFoundError') {
      log(`${mi('error')} Connect failed: ${err.message}`, 'error');
    }
  }
});

disconnectBtn.addEventListener('click', async () => {
  await closePort();
  portBadge.textContent = 'Not connected';
  portBadge.classList.remove('connected');
  connectBtn.style.display = '';
  disconnectBtn.style.display = 'none';
  if (state.mode === 'crosspoint') crosspointEraseBtn.disabled = true;
  updateFlashButton();
});

async function closePort() {
  try {
    if (state.transport) await state.transport.disconnect();
    if (state.port) await state.port.close();
  } catch (_) { }
  state.transport = null;
  state.port = null;
  state.loader = null;
}

function getPortName(port) {
  const info = port.getInfo?.() || {};
  if (info.usbVendorId) return `USB 0x${info.usbVendorId.toString(16).padStart(4, '0')}:0x${info.usbProductId?.toString(16).padStart(4, '0') ?? '????'}`;
  return 'Serial port';
}

/* ── Flash Button State ─────────────────────────────────────────── */
function updateFlashButton() {
  if (state.flashing) { flashBtn.disabled = true; return; }
  const hasPort = !!state.port;
  const hasTarget = !!state.target;
  const hasMode = !!state.mode;
  const hasVersion = state.mode === 'crosspoint' ? true : !!versionSelect.value;
  const hasCpBin = state.mode !== 'crosspoint' ? true : !!state.crosspointBinaryData;
  flashBtn.disabled = !(hasPort && hasTarget && hasMode && hasVersion && hasCpBin);
}

step4Back.addEventListener('click', () => goToStep(3));

/* ── Flash Sequence ─────────────────────────────────────────────── */
flashBtn.addEventListener('click', startFlash);
flashAgainBtn.addEventListener('click', () => {
  flashDone.style.display = 'none';
  progressSection.style.display = 'none';
  progressBar.style.width = '0%';
  progressBar.classList.remove('complete');
  flashLog.textContent = '';
  flashBtn.disabled = false;
});

async function initLoader() {
  if (!state.loader) {
    const terminal = {
      clean: () => { flashLog.textContent = ''; },
      writeLine: (s) => log(s),
      write: (s) => log(s, 'raw'),
    };
    state.loader = new ESPLoader({
      transport: state.transport,
      baudrate: FLASH_BAUD,
      romBaudrate: 115200,
      terminal,
    });
  }
  return state.loader;
}

async function startFlash() {
  if (state.flashing) return;
  state.flashing = true;
  flashBtn.disabled = true;
  step4Back.disabled = true;
  flashDone.style.display = 'none';
  progressSection.style.display = '';
  flashLog.textContent = '';

  try {
    // Detect chip
    log(`${mi('bolt')} Connecting to chip…`, 'info');
    const loader = await initLoader();
    const chip = await loader.main();
    log(`${mi('check_circle')} Chip detected: ${chip || 'connected'}`, 'info');

    if (state.mode === 'crosspoint') {
      await flashCrosspoint(loader);
    } else {
      await flashFromManifest(loader);
    }

    progressBar.style.width = '100%';
    progressBar.classList.add('complete');
    progressLabel.textContent = 'Done!';
    progressPct.textContent = '100%';
    log(`${mi('check_circle')} All done! Device is rebooting.`, 'success');
    flashDone.style.display = '';

    await closePort();
    portBadge.textContent = 'Not connected';
    portBadge.classList.remove('connected');
    connectBtn.style.display = '';
    disconnectBtn.style.display = 'none';

  } catch (err) {
    log(`${mi('error')} Flash failed: ${err.message}`, 'error');
    console.error(err);
    flashBtn.disabled = false;
  } finally {
    state.flashing = false;
    step4Back.disabled = false;
    updateFlashButton();
  }
}

async function flashFromManifest(loader) {
  // Load manifest if not cached
  if (!state.manifest) {
    log(`${mi('download')} Fetching manifest…`, 'info');
    let res = await fetch('../manifest.json').catch(() => null);
    if (!res || !res.ok) {
      res = await fetch('manifest.json').catch(() => null);
    }
    if (!res || !res.ok) {
      res = await fetch('https://t0mg.github.io/eenk/manifest.json').catch(() => null);
    }
    if (!res || !res.ok) {
      throw new Error('Failed to fetch manifest.json');
    }
    state.manifest = await res.json();
  }

  const entry = state.manifest?.targets?.[state.target]?.[state.mode];
  if (!entry || !entry.binaries?.length) {
    throw new Error(`No binaries defined in manifest for ${state.target}/${state.mode}`);
  }

  const fileArray = [];
  const totalBinaries = entry.binaries.length;

  for (let i = 0; i < totalBinaries; i++) {
    const { offset, url } = entry.binaries[i];
    log(`${mi('download')} Downloading binary ${i + 1}/${totalBinaries}…`, 'info');
    const buffer = await fetchBinaryAsArrayBuffer(url, (pct) => {
      const overall = ((i / totalBinaries) + (pct / 100 / totalBinaries)) * 40;
      setProgress(Math.round(overall), `Downloading ${i + 1}/${totalBinaries}…`);
    });
    fileArray.push({
      data: loader.ui8ToBstr(new Uint8Array(buffer)),
      address: typeof offset === 'string' ? parseInt(offset, 16) : offset
    });
  }

  log(`${mi('bolt')} Flashing binaries to device…`, 'info');
  await loader.writeFlash({
    fileArray,
    flashSize: 'keep',
    eraseAll: state.mode === 'factory',
    compress: true,
    reportProgress: (fileIdx, written, total) => {
      const filePct = total > 0 ? (written / total) : 0;
      const overall = 40 + ((fileIdx / totalBinaries) + (filePct / totalBinaries)) * 60;
      setProgress(Math.min(100, Math.round(overall)), `Flashing ${fileIdx + 1}/${totalBinaries}…`);
    }
  });

  log(`${mi('check_circle')} All binaries written.`, 'success');
  await loader.hardReset();
}

async function flashCrosspoint(loader) {
  if (!state.crosspointBinaryData) {
    throw new Error('No CrossPoint binary selected.');
  }
  log(`${mi('delete')} Erasing flash & restoring stock firmware…`, 'info');
  setProgress(10, 'Writing stock firmware…');

  const fileArray = [{
    data: loader.ui8ToBstr(new Uint8Array(state.crosspointBinaryData)),
    address: 0x0
  }];

  await loader.writeFlash({
    fileArray,
    flashSize: 'keep',
    eraseAll: true,
    compress: true,
    reportProgress: (fileIdx, written, total) => {
      const pct = total > 0 ? Math.round((written / total) * 100) : 0;
      setProgress(10 + Math.round(pct * 0.85), `Writing stock firmware (${pct}%)…`);
    }
  });

  log(`${mi('check_circle')} Stock firmware written.`, 'success');
  await loader.hardReset();
}

/* ── Fetch binary with progress ─────────────────────────────────── */
async function fetchBinaryAsArrayBuffer(url, onProgress) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`Failed to fetch ${url.split('/').pop()}: HTTP ${res.status}`);
  const total = parseInt(res.headers.get('content-length') || '0', 10);
  const reader = res.body.getReader();
  const chunks = [];
  let received = 0;

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    received += value.length;
    if (total > 0 && onProgress) onProgress(Math.round(received / total * 100));
  }

  const buffer = new Uint8Array(received);
  let pos = 0;
  for (const chunk of chunks) {
    buffer.set(chunk, pos);
    pos += chunk.length;
  }
  return buffer.buffer;
}

function arrayBufferToBase64(buffer) {
  const bytes = new Uint8Array(buffer);
  let binary = '';
  for (let i = 0; i < bytes.byteLength; i++) binary += String.fromCharCode(bytes[i]);
  return btoa(binary);
}

/* ── Log & Progress helpers ─────────────────────────────────────── */
function mi(icon) {
  return `<span class="material-symbols-outlined" style="font-size:1.05em;vertical-align:-2px;margin-right:4px;">${icon}</span>`;
}

function log(msg, type = 'default') {
  const line = document.createElement('span');
  line.innerHTML = msg + '\n';
  if (type === 'success') line.className = 'log-success';
  else if (type === 'error') line.className = 'log-error';
  else if (type === 'info') line.className = 'log-info';
  flashLog.appendChild(line);
  flashLog.scrollTop = flashLog.scrollHeight;
}

function setProgress(pct, label) {
  const clamped = Math.max(0, Math.min(100, pct));
  progressBar.style.width = `${clamped}%`;
  progressBar.setAttribute('aria-valuenow', clamped);
  if (label) progressLabel.textContent = label;
  progressPct.textContent = `${clamped}%`;
}

/* ── Theme sync with eenky ──────────────────────────────────────── */
if (IS_ELECTRON) {
  // Listen for theme messages from eenky parent frame
  window.addEventListener('message', (e) => {
    if (e.data?.type === 'change-theme') {
      document.body.classList.remove('theme-dark', 'theme-light');
      document.body.classList.add(`theme-${e.data.theme}`);
    }
  });
}

/* ── Init ───────────────────────────────────────────────────────── */
(function init() {
  goToStep(1);

  // Check Web Serial availability and warn if missing
  if (!('serial' in navigator)) {
    const warning = document.createElement('div');
    warning.style.cssText = 'background:#FF4D00;color:#fff;padding:0.75rem 1.5rem;font-size:0.85rem;border-bottom:2px solid #111;font-family:Inter,sans-serif;';
    warning.innerHTML = `${mi('warning')} Web Serial API not available in this browser. Please use Chrome or Edge (v89+), or open this page from the eenky app.`;
    document.body.prepend(warning);
    connectBtn.disabled = true;
  }
})();
