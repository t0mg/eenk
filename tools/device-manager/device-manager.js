/**
 * device-manager.js — eenk Device Manager orchestration
 *
 * Portable vanilla JS — zero framework dependencies.
 * Mirrors the logic from eenky's deviceStore.js (Pinia) and DeviceApp.vue.
 *
 * Requires: serial-protocol.js (loaded first, defines global EenkSerialProtocol)
 *
 * Runs in:
 *   - Chrome / Edge browser (GitHub Pages)
 *   - Electron iframe (eenky Device Manager window)
 *
 * Sidecar handling:
 *   - Electron: uses window.api.fs + window.api.path (preload bridge) to
 *     auto-discover .epdfont/.media files alongside the selected .bin
 *   - Browser: shows an "associated files" prompt with a secondary file picker
 */

'use strict';

/* ── Context detection ──────────────────────────────────────────── */
const IS_ELECTRON = (
  (typeof window !== 'undefined' && typeof window.api !== 'undefined') ||
  (typeof navigator !== 'undefined' && /Electron/.test(navigator.userAgent))
);

const IS_IFRAME = typeof window !== 'undefined' && window.self !== window.top;

const HAS_FS_API = IS_ELECTRON && typeof window.api?.fs !== 'undefined';

if (IS_ELECTRON) document.body.classList.add('electron-embed');
if (IS_IFRAME) document.body.classList.add('iframe-embed');

/* ── State ──────────────────────────────────────────────────────── */
const state = {
  isConnected: false,
  isConnecting: false,
  protocolVersion: null,
  sdInfo: { total: 0, used: 0, free: 0 },
  stories: [],
  books: [],
  saves: [],
  transferState: null,
  error: null,
  currentTab: 'stories',
};

let protocol = null;

/** Merge patch into state then re-render the affected parts. */
function setState(patch) {
  Object.assign(state, patch);
  render();
}

/* ── DOM references ─────────────────────────────────────────────── */
const $ = id => document.getElementById(id);

const connectBtn = $('connect-btn');
const disconnectBtn = $('disconnect-btn');
const refreshBtn = $('refresh-btn');
const statusDot = $('status-dot');
const statusText = $('status-text');
const sdBarRow = $('sd-bar-row');
const sdBar = $('sd-bar');
const sdSizeText = $('sd-size-text');
const emptyState = $('empty-state');
const panelStories = $('panel-stories');
const panelBooks = $('panel-books');
const panelSaves = $('panel-saves');
const storiesList = $('stories-list');
const booksList = $('books-list');
const savesList = $('saves-list');
const uploadInput = $('upload-input');
const uploadZone = $('upload-zone');
const uploadBookInput = $('upload-book-input');
const uploadBookZone = $('upload-book-zone');
const sidecarPrompt = $('sidecar-prompt');
const sidecarFileList = $('sidecar-file-list');
const sidecarUploadBtn = $('sidecar-upload-btn');
const sidecarSkipBtn = $('sidecar-skip-btn');
const transferFooter = $('transfer-footer');
const transferIcon = $('transfer-icon');
const transferFilename = $('transfer-filename');
const transferPct = $('transfer-pct');
const transferBar = $('transfer-bar');
const confirmModal = $('confirm-modal');
const confirmText = $('confirm-text');
const confirmOk = $('confirm-ok');
const confirmCancel = $('confirm-cancel');
const errorBanner = $('error-banner');
const errorText = $('error-text');

/* ── Pending upload context (for sidecar prompt) ─────────────────── */
let _pendingBin = null;  // { file, data, folderName }
let _pendingSidecars = [];    // Array<File> from browser picker

/* ── Render ─────────────────────────────────────────────────────── */
function render() {
  const { isConnected, isConnecting, sdInfo, stories, books, saves, transferState, error, currentTab } = state;

  // Header status
  statusDot.className = 'status-dot' + (isConnecting ? ' connecting' : isConnected ? ' connected' : '');
  statusText.textContent = isConnecting
    ? 'Connecting…'
    : isConnected
      ? `v${state.protocolVersion ?? '?'}`
      : 'Not connected';

  connectBtn.style.display = isConnected || isConnecting ? 'none' : '';
  disconnectBtn.style.display = isConnected ? '' : 'none';
  refreshBtn.style.display = isConnected ? '' : 'none';
  connectBtn.disabled = isConnecting;

  // SD bar
  sdBarRow.style.visibility = isConnected ? '' : 'hidden';
  if (isConnected && sdInfo.total > 0) {
    const pct = Math.round((sdInfo.used / sdInfo.total) * 100);
    sdBar.style.width = `${pct}%`;
    sdSizeText.textContent = `${formatSize(sdInfo.used)} / ${formatSize(sdInfo.total)}`;
  }

  // Content area
  const showFiles = isConnected && !isConnecting;
  emptyState.style.display = showFiles ? 'none' : '';
  panelStories.style.display = showFiles && currentTab === 'stories' ? 'block' : 'none';
  if (panelBooks) panelBooks.style.display = showFiles && currentTab === 'books' ? 'block' : 'none';
  panelSaves.style.display = showFiles && currentTab === 'saves' ? 'block' : 'none';

  // Tab strip active state
  document.querySelectorAll('.dm-tab').forEach(btn => {
    const active = btn.dataset.tab === currentTab;
    btn.classList.toggle('active', active);
    btn.setAttribute('aria-selected', active.toString());
  });

  // File lists
  renderFileList(storiesList, stories, 'story');
  if (booksList) renderFileList(booksList, books, 'book');
  renderFileList(savesList, saves, 'save');

  // Transfer footer
  if (transferState) {
    transferFooter.classList.add('visible');
    transferIcon.innerHTML = transferState.type === 'upload'
      ? '<span class="material-symbols-outlined" style="vertical-align:middle;">upload</span>'
      : '<span class="material-symbols-outlined" style="vertical-align:middle;">download</span>';
    transferFilename.textContent = transferState.filename || '…';
    const pct = transferState.bytesTotal > 0
      ? Math.round((transferState.bytesTransferred / transferState.bytesTotal) * 100)
      : 0;
    transferBar.style.width = `${pct}%`;
    transferPct.textContent = transferState.bytesTotal > 0
      ? `${formatSize(transferState.bytesTransferred)} / ${formatSize(transferState.bytesTotal)}`
      : '…';
  } else {
    transferFooter.classList.remove('visible');
  }

  // Error
  if (error) {
    errorBanner.style.display = '';
    errorText.textContent = error;
  } else {
    errorBanner.style.display = 'none';
  }
}

function renderFileList(container, items, kind) {
  if (!container) return;
  container.innerHTML = '';
  if (!items.length) {
    const plural = kind === 'story' ? 'stories' : `${kind}s`;
    container.innerHTML = `<div class="empty-list">No ${plural} found on device.</div>`;
    return;
  }
  items.forEach(item => {
    const div = document.createElement('div');
    div.className = 'file-item';
    const icon = item.type === 'D' ? '<span class="material-symbols-outlined">folder</span>' : '<span class="material-symbols-outlined">description</span>';
    div.innerHTML = `
      <span class="file-icon" aria-hidden="true">${icon}</span>
      <span class="file-name" title="${item.path}">${item.name}</span>
      <span class="file-size">${item.size ? formatSize(item.size) : ''}</span>
      <div class="file-actions">
        ${(item.type === 'D' || kind === 'book') ? '' : `<button class="icon-btn download-btn" data-path="${item.path}" data-name="${item.name}" aria-label="Download ${item.name}">
          <span class="material-symbols-outlined" style="font-size:1em;">download</span> Download
        </button>`}
        <button class="icon-btn delete-btn delete" data-path="${item.path}" data-name="${item.name}" aria-label="Delete ${item.name}">
          <span class="material-symbols-outlined" style="font-size:1em;">delete</span> Delete
        </button>
      </div>`;
    container.appendChild(div);
  });
}

/* ── Helpers ────────────────────────────────────────────────────── */
function formatSize(bytes) {
  if (!bytes && bytes !== 0) return '';
  if (bytes === 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(1024));
  return `${(bytes / Math.pow(1024, i)).toFixed(1)} ${units[i]}`;
}

function setTransfer(type, filename, bytesTransferred = 0, bytesTotal = 0) {
  setState({ transferState: { type, filename, bytesTransferred, bytesTotal } });
}

function clearTransfer() { setState({ transferState: null }); }

function showError(msg) { setState({ error: msg }); }
function clearError() { setState({ error: null }); }

/* ── Protocol actions ───────────────────────────────────────────── */
async function connect() {
  if (!('serial' in navigator)) return;
  setState({ isConnecting: true, error: null });
  try {
    protocol = new EenkSerialProtocol();
    const res = await protocol.connect();
    setState({ isConnected: true, isConnecting: false, protocolVersion: res.version });
    await refreshFiles();
  } catch (e) {
    protocol = null;
    setState({ isConnected: false, isConnecting: false, error: e.message });
  }
}

async function disconnect() {
  if (protocol) {
    try { await protocol.disconnect(); } catch (_) { }
    protocol = null;
  }
  setState({
    isConnected: false, protocolVersion: null,
    stories: [], books: [], saves: [], sdInfo: { total: 0, used: 0, free: 0 },
    transferState: null, error: null,
  });
}

async function refreshFiles() {
  if (!state.isConnected || !protocol) return;
  try {
    const sdInfo = await protocol.getInfo();
    setState({ sdInfo });

    let stories = [];
    try {
      const files = await protocol.listFiles('/stories');
      stories = files.map(f => ({ ...f, path: `/stories/${f.name}` }));
    } catch (e) {
      if (!e.message?.includes('NOT_FOUND')) throw e;
    }

    let books = [];
    try {
      const files = await protocol.listFiles('/books');
      books = files.map(f => ({ ...f, path: `/books/${f.name}` }));
    } catch (e) {
      if (!e.message?.includes('NOT_FOUND')) throw e;
    }

    let saves = [];
    try {
      const files = await protocol.listFiles('/.eenk_saves');
      saves = files.map(f => ({ ...f, path: `/.eenk_saves/${f.name}` }));
    } catch (e) {
      if (!e.message?.includes('NOT_FOUND')) throw e;
    }

    setState({ stories, books, saves });
  } catch (e) {
    showError('Error refreshing files: ' + e.message);
  }
}

async function deleteItem(path, name) {
  if (!state.isConnected || !protocol) return;
  try {
    await protocol.deleteFile(path);

    // If deleting an epub book or a file in /books, also clean up the related .eenk_cache subfolder
    if (path.startsWith('/books/') || name.toLowerCase().endsWith('.epub')) {
      const stem = name.replace(/\.[^/.]+$/, '');
      if (stem) {
        try {
          await protocol.deleteFile(`/.eenk_cache/${stem}`);
        } catch (_) {
          // Cache subfolder may not exist if book was never opened
        }
      }
    }

    await refreshFiles();
  } catch (e) {
    showError(`Delete failed: ${e.message}`);
  }
}

async function downloadFile(path, filename) {
  if (!state.isConnected || !protocol) return;
  try {
    setTransfer('download', filename);
    const data = await protocol.downloadFile(path, (transferred, total) => {
      setState({ transferState: { type: 'download', filename, bytesTransferred: transferred, bytesTotal: total } });
    });
    // Trigger browser download
    const blob = new Blob([data]);
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  } catch (e) {
    showError('Download failed: ' + e.message);
  } finally {
    clearTransfer();
  }
}

/** Upload a .bin and optional sidecars. Called after sidecar decision is made. */
async function executeUpload(binFile, binData, sidecars) {
  if (!state.isConnected || !protocol) return;
  try {
    // Parse eenk header magic (bytes 0-3) to extract story title for folder name
    let title = binFile.name.replace(/\.bin$/i, '');
    if (binData.length >= 72) {
      const magic = new TextDecoder().decode(binData.slice(0, 4));
      if (magic === 'eenk') {
        const titleBytes = binData.slice(8, 72);
        const endIdx = titleBytes.indexOf(0);
        if (endIdx > 0) title = new TextDecoder().decode(titleBytes.slice(0, endIdx));
      }
    }
    const folderName = title.toLowerCase().replace(/[^a-z0-9]/g, '_').substring(0, 32);

    try { await protocol.mkdir(`/stories/${folderName}`); } catch (_) { }

    // Upload main .bin
    setState({ transferState: { type: 'upload', filename: binFile.name, bytesTransferred: 0, bytesTotal: binData.length } });
    await protocol.uploadFile(`/stories/${folderName}/main.bin`, binData, (transferred, total) => {
      setState({ transferState: { type: 'upload', filename: binFile.name, bytesTransferred: transferred, bytesTotal: total } });
    });

    // Upload sidecars
    for (const sidecar of sidecars) {
      const buf = await sidecar.arrayBuffer();
      const data = new Uint8Array(buf);
      setState({ transferState: { type: 'upload', filename: sidecar.name, bytesTransferred: 0, bytesTotal: data.length } });
      await protocol.uploadFile(`/stories/${folderName}/${sidecar.name}`, data, (transferred, total) => {
        setState({ transferState: { type: 'upload', filename: sidecar.name, bytesTransferred: transferred, bytesTotal: total } });
      });
    }

    await refreshFiles();
  } catch (e) {
    showError('Upload failed: ' + e.message);
  } finally {
    clearTransfer();
    _pendingBin = null;
    _pendingSidecars = [];
    sidecarPrompt.style.display = 'none';
  }
}

/** Handle a .bin file selection — discover sidecars then decide next step. */
async function handleBinSelected(binFile, providedFiles = []) {
  const buf = await binFile.arrayBuffer();
  const binData = new Uint8Array(buf);
  _pendingBin = { file: binFile, data: binData };

  // Validate magic byte
  if (binData.length < 4 || new TextDecoder().decode(binData.slice(0, 4)) !== 'eenk') {
    showError('This does not appear to be a valid eenk story file (missing magic header).');
    return;
  }

  if (HAS_FS_API && binFile.path) {
    // Electron: auto-discover sidecars via filesystem API
    try {
      const SIDECAR_EXTS = ['.epdfont', '.media'];
      const dirPath = await window.api.path.dirname(binFile.path);
      const baseName = await window.api.path.basename(binFile.path, '.bin');
      const entries = await window.api.fs.readdir(dirPath);
      const found = [];

      for (const entry of entries) {
        const ext = await window.api.path.extname(entry);
        if (SIDECAR_EXTS.includes(ext) && (ext === '.epdfont' || entry.startsWith(baseName))) {
          const fullPath = await window.api.path.join(dirPath, entry);
          const rawBuf = await window.api.fs.readFile(fullPath);
          // Wrap in a pseudo-File so executeUpload can call .arrayBuffer()
          found.push({ name: entry, arrayBuffer: () => Promise.resolve(rawBuf.buffer ?? rawBuf) });
        }
      }
      // Skip prompt — upload everything automatically (matches existing eenky behaviour)
      await executeUpload(binFile, binData, found);
    } catch (e) {
      console.warn('[device-manager] Sidecar scan failed:', e);
      await executeUpload(binFile, binData, []);
    }
  } else {
    // Browser: no filesystem access — check if sidecars were provided in the same drop/selection
    const SIDECAR_EXTS = ['.epdfont', '.media'];
    const baseName = binFile.name.replace(/\.bin$/i, '');

    // Auto-detect from providedFiles
    const autoFound = providedFiles.filter(f => {
      const ext = f.name.substring(f.name.lastIndexOf('.')).toLowerCase();
      return f !== binFile && SIDECAR_EXTS.includes(ext) && (ext === '.epdfont' || f.name.startsWith(baseName));
    });

    _pendingSidecars = autoFound;
    sidecarFileList.innerHTML = '';
    sidecarPrompt.style.display = '';

    const titleEl = $('sidecar-title');
    const descEl = $('sidecar-desc');

    if (autoFound.length > 0) {
      titleEl.textContent = 'Associated files detected';
      descEl.textContent = 'The following files were provided alongside the story. Upload them?';

      autoFound.forEach(f => {
        const row = document.createElement('div');
        row.className = 'sidecar-file-row';
        row.textContent = f.name;
        sidecarFileList.appendChild(row);
      });
    } else {
      titleEl.textContent = 'Associated files?';
      descEl.textContent = 'If your story uses custom fonts (.epdfont) or images (.media), you must select them now to upload them alongside the story.';
    }

    // Always offer an "add sidecars" file picker inside the prompt
    const label = document.createElement('label');
    label.className = 'btn btn-secondary';
    label.style.marginTop = autoFound.length > 0 ? '0.5rem' : '0.25rem';
    label.textContent = '+ Add .epdfont / .media files…';
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.epdfont,.media';
    input.multiple = true;
    input.style.display = 'none';
    input.addEventListener('change', () => {
      // Append manually picked files
      const newFiles = Array.from(input.files || []);
      if (newFiles.length > 0) {
        // Only keep unique files by name
        const existingNames = new Set(_pendingSidecars.map(f => f.name));
        const uniqueNew = newFiles.filter(f => !existingNames.has(f.name));

        _pendingSidecars = [..._pendingSidecars, ...uniqueNew];
        // Re-render list
        sidecarFileList.innerHTML = '';
        _pendingSidecars.forEach(f => {
          const row = document.createElement('div');
          row.className = 'sidecar-file-row';
          row.textContent = f.name;
          sidecarFileList.appendChild(row);
        });
      }
    });
    label.appendChild(input);
    sidecarFileList.appendChild(label);
  }
}

/* ── Confirm modal ──────────────────────────────────────────────── */
let _pendingDelete = null;

function promptDelete(path, name) {
  _pendingDelete = { path, name };
  confirmText.textContent = `Delete "${name}"? This cannot be undone.`;
  confirmModal.style.display = '';
}

confirmOk.addEventListener('click', async () => {
  confirmModal.style.display = 'none';
  if (_pendingDelete) {
    await deleteItem(_pendingDelete.path, _pendingDelete.name);
    _pendingDelete = null;
  }
});

confirmCancel.addEventListener('click', () => {
  confirmModal.style.display = 'none';
  _pendingDelete = null;
});

/* ── Event wiring ───────────────────────────────────────────────── */
connectBtn.addEventListener('click', connect);
disconnectBtn.addEventListener('click', disconnect);
refreshBtn.addEventListener('click', refreshFiles);

// Tabs
document.querySelectorAll('.dm-tab').forEach(btn => {
  btn.addEventListener('click', () => {
    setState({ currentTab: btn.dataset.tab });
  });
});

async function executeBookUpload(epubFile) {
  if (!state.isConnected || !protocol) return;
  try {
    try { await protocol.mkdir('/books'); } catch (_) { }

    const buf = await epubFile.arrayBuffer();
    const data = new Uint8Array(buf);

    setState({ transferState: { type: 'upload', filename: epubFile.name, bytesTransferred: 0, bytesTotal: data.length } });
    await protocol.uploadFile(`/books/${epubFile.name}`, data, (transferred, total) => {
      setState({ transferState: { type: 'upload', filename: epubFile.name, bytesTransferred: transferred, bytesTotal: total } });
    });

    await refreshFiles();
  } catch (e) {
    showError(`Upload failed for ${epubFile.name}: ` + e.message);
  } finally {
    clearTransfer();
  }
}

async function handleEpubSelected(epubFiles) {
  if (!epubFiles || !epubFiles.length) return;
  for (const epub of epubFiles) {
    await executeBookUpload(epub);
  }
}

async function extractDroppedFiles(e) {
  const files = [];

  if (e.dataTransfer?.items) {
    const items = Array.from(e.dataTransfer.items);
    for (const item of items) {
      if (item.kind === 'file') {
        const entry = item.webkitGetAsEntry ? item.webkitGetAsEntry() : null;
        if (entry) {
          if (entry.isDirectory) {
            const readDir = async (dirEntry) => {
              const reader = dirEntry.createReader();
              let allEntries = [];
              let readEntries;
              do {
                readEntries = await new Promise(resolve => reader.readEntries(resolve, () => resolve([])));
                allEntries.push(...readEntries);
              } while (readEntries.length > 0);

              for (const en of allEntries) {
                if (en.isFile) {
                  const file = await new Promise(resolve => en.file(resolve, () => resolve(null)));
                  if (file) files.push(file);
                } else if (en.isDirectory) {
                  await readDir(en);
                }
              }
            };
            await readDir(entry);
          } else {
            const file = item.getAsFile();
            if (file) files.push(file);
          }
        } else {
          const file = item.getAsFile();
          if (file) files.push(file);
        }
      }
    }
  } else {
    files.push(...Array.from(e.dataTransfer?.files || []));
  }

  return files;
}

// Upload input (Stories)
uploadInput.addEventListener('change', async () => {
  const files = Array.from(uploadInput.files || []);
  uploadInput.value = '';
  const bin = files.find(f => f.name.endsWith('.bin'));
  if (bin) await handleBinSelected(bin, files);
  else if (files.length > 0) showError("Please include a .bin story file.");
});

// Upload zone click → trigger hidden input (Stories)
uploadZone.addEventListener('click', () => uploadInput.click());
uploadZone.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') uploadInput.click(); });

// Drag-and-drop on upload zone (Stories)
uploadZone.addEventListener('dragover', e => { e.preventDefault(); uploadZone.classList.add('drag-over'); });
uploadZone.addEventListener('dragleave', () => uploadZone.classList.remove('drag-over'));
uploadZone.addEventListener('drop', async e => {
  e.preventDefault();
  uploadZone.classList.remove('drag-over');

  const files = await extractDroppedFiles(e);
  const bin = files.find(f => f.name.endsWith('.bin'));
  if (bin) await handleBinSelected(bin, files);
  else if (files.length > 0) showError("Please include a .bin story file.");
});

// Upload input (Books)
if (uploadBookInput) {
  uploadBookInput.addEventListener('change', async () => {
    const files = Array.from(uploadBookInput.files || []);
    uploadBookInput.value = '';
    const epubs = files.filter(f => f.name.toLowerCase().endsWith('.epub'));
    if (epubs.length > 0) {
      await handleEpubSelected(epubs);
    } else if (files.length > 0) {
      showError("Please select a .epub book file.");
    }
  });
}

// Upload zone click → trigger hidden input (Books)
if (uploadBookZone) {
  uploadBookZone.addEventListener('click', () => uploadBookInput?.click());
  uploadBookZone.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') uploadBookInput?.click(); });

  // Drag-and-drop on upload zone (Books)
  uploadBookZone.addEventListener('dragover', e => { e.preventDefault(); uploadBookZone.classList.add('drag-over'); });
  uploadBookZone.addEventListener('dragleave', () => uploadBookZone.classList.remove('drag-over'));
  uploadBookZone.addEventListener('drop', async e => {
    e.preventDefault();
    uploadBookZone.classList.remove('drag-over');

    const files = await extractDroppedFiles(e);
    const epubs = files.filter(f => f.name.toLowerCase().endsWith('.epub'));
    if (epubs.length > 0) {
      await handleEpubSelected(epubs);
    } else if (files.length > 0) {
      showError("Please include a .epub book file.");
    }
  });
}

// Sidecar prompt buttons
sidecarUploadBtn.addEventListener('click', async () => {
  sidecarPrompt.style.display = 'none';
  if (_pendingBin) await executeUpload(_pendingBin.file, _pendingBin.data, _pendingSidecars);
});

sidecarSkipBtn.addEventListener('click', async () => {
  sidecarPrompt.style.display = 'none';
  if (_pendingBin) await executeUpload(_pendingBin.file, _pendingBin.data, []);
});

// File list actions (download / delete) — event delegation
document.addEventListener('click', e => {
  const dlBtn = e.target.closest('.download-btn');
  if (dlBtn) {
    downloadFile(dlBtn.dataset.path, dlBtn.dataset.name);
    return;
  }
  const delBtn = e.target.closest('.delete-btn');
  if (delBtn) {
    promptDelete(delBtn.dataset.path, delBtn.dataset.name);
  }
});

/* ── Theme sync with eenky ──────────────────────────────────────── */
window.addEventListener('message', e => {
  if (e.data?.type === 'change-theme') {
    document.body.classList.remove('theme-dark', 'theme-light');
    document.body.classList.add(`theme-${e.data.theme}`);
  }
});

/* ── Web Serial availability check ─────────────────────────────── */
(function init() {
  if (!('serial' in navigator)) {
    $('serial-warn').style.display = '';
    connectBtn.disabled = true;
  }
  render();
})();
