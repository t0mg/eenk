/**
 * device-manager.js — eenk Device Manager orchestration
 *
 * Portable vanilla JS — zero framework dependencies.
 * Mirrors the logic from eenky's deviceStore.js (Pinia) and DeviceApp.vue.
 *
 * Requires: serial-protocol.js (loaded first, defines global EenkSerialProtocol)
 *           zip-unpacker.js   (defines global unpackZip)
 *
 * Runs in:
 *   - Chrome / Edge browser (GitHub Pages)
 *   - Electron iframe (eenky Device Manager window)
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

  // Preloaded Story State (from .eenk package, catalog, or drag-and-drop)
  preloadedStory: null, // { title, author, compileTime, fontName, folderName, binData, binName, sidecars: [{name, data}], coverBitmap, coverImgUrl, rawBlobUrl, sourceFileName }
  pendingExternalUrl: null,
  externalHost: null,
  uploadSuccess: null, // { title }
  isPreloadUploading: false,
  isConfirming: false,
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
const confirmHeading = $('confirm-heading');
const confirmText = $('confirm-text');
const confirmOk = $('confirm-ok');
const confirmCancel = $('confirm-cancel');
const errorBanner = $('error-banner');
const errorText = $('error-text');

// Upload Success Elements
const uploadSuccessCard = $('upload-success-card');
const successStoryTitle = $('success-story-title');
const successStoryDesc = $('success-story-desc');
const successDisconnectBtn = $('success-disconnect-btn');
const successContinueBtn = $('success-continue-btn');

// Preload & Consent Elements
const externalConsentCard = $('external-consent-card');
const consentHost = $('consent-host');
const consentProceedBtn = $('consent-proceed-btn');
const consentDismissBtn = $('consent-dismiss-btn');

const inspectProgressCard = $('inspect-progress-card');
const inspectStatusText = $('inspect-status-text');
const inspectProgressBar = $('inspect-progress-bar');

const storyPreloadCard = $('story-preload-card');
const preloadCoverCanvas = $('preload-cover-canvas');
const preloadCoverImg = $('preload-cover-img');
const preloadCoverPlaceholder = $('preload-cover-placeholder');
const preloadTitle = $('preload-title');
const preloadAuthor = $('preload-author');
const preloadInventory = $('preload-inventory');
const preloadSaveWarning = $('preload-save-warning');
const preloadWarningText = $('preload-warning-text');
const preloadInstallBtn = $('preload-install-btn');
const preloadDownloadBtn = $('preload-download-btn');
const preloadDismissBtn = $('preload-dismiss-btn');

/* ── Pending upload context (for manual browser sidecar prompt) ──── */
let _pendingBin = null;  // { file, data, folderName }
let _pendingSidecars = [];    // Array<File> from browser picker

/* ── Render ─────────────────────────────────────────────────────── */
function render() {
  const { isConnected, isConnecting, sdInfo, stories, books, saves, transferState, error, currentTab, preloadedStory, pendingExternalUrl } = state;

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

  // Consent card
  if (externalConsentCard) {
    if (pendingExternalUrl) {
      externalConsentCard.style.display = '';
      if (consentHost) consentHost.textContent = state.externalHost || '';
    } else {
      externalConsentCard.style.display = 'none';
    }
  }

  // Preloaded story card
  if (storyPreloadCard) {
    if (preloadedStory && !pendingExternalUrl) {
      storyPreloadCard.style.display = '';
      preloadTitle.textContent = preloadedStory.title;
      preloadAuthor.textContent = `By ${preloadedStory.author || 'Unknown'}`;

      const totalBytes = (preloadedStory.binData?.length || 0) +
        (preloadedStory.sidecars || []).reduce((acc, s) => acc + (s.data?.length || 0), 0);
      const fileCount = 1 + (preloadedStory.sidecars?.length || 0);
      const filesStr = `${fileCount} ${fileCount === 1 ? 'file' : 'files'} (${formatSize(totalBytes)})`;

      let buildStr = '';
      if (preloadedStory.compileTime) {
        const d = new Date(preloadedStory.compileTime * 1000);
        buildStr = `Built on ${d.toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' })}`;
      }

      const metaParts = [filesStr];
      if (buildStr) metaParts.push(buildStr);
      preloadInventory.textContent = metaParts.join(' • ');

      // Cover rendering: prioritize authentic 1-bit decoded bitmap from .media, then image URL, then placeholder
      if (preloadedStory.coverBitmap) {
        render1BitCoverToCanvas(preloadCoverCanvas, preloadedStory.coverBitmap);
        preloadCoverCanvas.style.display = 'block';
        preloadCoverImg.style.display = 'none';
        preloadCoverPlaceholder.style.display = 'none';
      } else if (preloadedStory.coverImgUrl) {
        preloadCoverImg.src = preloadedStory.coverImgUrl;
        preloadCoverImg.style.display = 'block';
        preloadCoverCanvas.style.display = 'none';
        preloadCoverPlaceholder.style.display = 'none';
      } else {
        preloadCoverPlaceholder.style.display = 'flex';
        preloadCoverCanvas.style.display = 'none';
        preloadCoverImg.style.display = 'none';
      }

      // Existing story & save collision check
      const storyExists = isConnected && stories.some(s => s.name === preloadedStory.folderName || s.path === `/stories/${preloadedStory.folderName}`);
      if (storyExists) {
        preloadSaveWarning.style.display = '';
        preloadInstallBtn.innerHTML = '<span class="material-symbols-outlined">bolt</span> Reinstall';
      } else {
        preloadSaveWarning.style.display = 'none';
        preloadInstallBtn.innerHTML = isConnected
          ? '<span class="material-symbols-outlined">bolt</span> Install'
          : '<span class="material-symbols-outlined">usb</span> Connect &amp; Install';
      }

      if (transferState && transferState.type === 'upload') {
        preloadInstallBtn.disabled = true;
        preloadInstallBtn.innerHTML = '<span class="material-symbols-outlined spin">sync</span> Uploading…';
        if (preloadDismissBtn) preloadDismissBtn.disabled = true;
      } else {
        preloadInstallBtn.disabled = isConnecting;
        if (preloadDismissBtn) preloadDismissBtn.disabled = isConnecting;
      }

      if (preloadedStory.rawBlobUrl && preloadDownloadBtn) {
        preloadDownloadBtn.style.display = '';
        preloadDownloadBtn.href = preloadedStory.rawBlobUrl;
        preloadDownloadBtn.download = preloadedStory.sourceFileName || `${preloadedStory.folderName}.eenk`;
      } else if (preloadDownloadBtn) {
        preloadDownloadBtn.style.display = 'none';
      }
    } else {
      storyPreloadCard.style.display = 'none';
    }
  }

  // Upload Success card
  if (uploadSuccessCard) {
    if (isConnected && state.uploadSuccess) {
      uploadSuccessCard.style.display = '';
      if (successStoryTitle) {
        successStoryTitle.textContent = `"${state.uploadSuccess.title}" Installed!`;
      }
    } else {
      uploadSuccessCard.style.display = 'none';
    }
  }

  // Content area
  const isPreloadOrPromptVisible = !!(preloadedStory || pendingExternalUrl || state.isPreloadUploading || state.isConfirming || state.uploadSuccess);
  const showFiles = isConnected && !isConnecting && !isPreloadOrPromptVisible;
  emptyState.style.display = (isConnected || isPreloadOrPromptVisible) ? 'none' : '';
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

function setInspectProgress(text, pct = 0) {
  if (inspectProgressCard) {
    inspectProgressCard.style.display = '';
    if (inspectStatusText) inspectStatusText.textContent = text;
    if (inspectProgressBar) inspectProgressBar.style.width = `${pct}%`;
  }
}

function hideInspectProgress() {
  if (inspectProgressCard) inspectProgressCard.style.display = 'none';
}

function cleanUrlParams() {
  if (typeof window !== 'undefined' && window.history?.replaceState) {
    const cleanUrl = window.location.pathname + window.location.hash;
    window.history.replaceState({}, document.title, cleanUrl);
  }
}

function dismissPreloadedStory() {
  if (state.preloadedStory?.coverImgUrl && state.preloadedStory.coverImgUrl.startsWith('blob:')) {
    URL.revokeObjectURL(state.preloadedStory.coverImgUrl);
  }
  if (state.preloadedStory?.rawBlobUrl) {
    URL.revokeObjectURL(state.preloadedStory.rawBlobUrl);
  }
  setState({ preloadedStory: null, pendingExternalUrl: null, externalHost: null });
  cleanUrlParams();
}

/* ── Binary Metadata & Cover Art Helpers ────────────────────────── */

// 32-bit FNV-1a hash matching eenk compiler
function fnv1a(str) {
  let hash = 2166136261;
  for (let i = 0; i < str.length; i++) {
    hash ^= str.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

/** Parse eenk StoryMetadata 128-byte header from beginning of .bin */
function parseStoryMetadata(binData) {
  if (!binData || binData.length < 128) return null;
  const magic = new TextDecoder('ascii').decode(binData.slice(0, 4));
  if (magic !== 'eenk') return null;

  const view = new DataView(binData.buffer, binData.byteOffset, binData.byteLength);
  const version = view.getUint16(4, true);
  if (version !== 1) return null;

  const titleBytes = binData.slice(8, 72);
  const titleEnd = titleBytes.indexOf(0);
  const title = new TextDecoder('utf-8').decode(titleEnd >= 0 ? titleBytes.slice(0, titleEnd) : titleBytes).trim() || 'Untitled Story';

  const authorBytes = binData.slice(72, 104);
  const authorEnd = authorBytes.indexOf(0);
  const author = new TextDecoder('utf-8').decode(authorEnd >= 0 ? authorBytes.slice(0, authorEnd) : authorBytes).trim() || 'Unknown Author';

  const compileTime = view.getUint32(104, true);
  const flags = view.getUint32(108, true);
  const fontLen = view.getUint8(112);
  let fontName = '';
  if (fontLen > 0 && fontLen <= 15) {
    fontName = new TextDecoder('utf-8').decode(binData.slice(113, 113 + fontLen)).trim();
  }

  return { title, author, compileTime, flags, fontName, hasMedia: (flags & 1) !== 0 };
}

/** Extract 1-bit Floyd-Steinberg cover bitmap from .media sidecar buffer */
function extractMediaCover(mediaData) {
  if (!mediaData || mediaData.length < 28) return null;
  const magic = new TextDecoder('ascii').decode(mediaData.slice(0, 4));
  if (magic !== 'ENKM') return null;

  const view = new DataView(mediaData.buffer, mediaData.byteOffset, mediaData.byteLength);
  const numEntries = view.getUint32(4, true);
  const thumbHash = fnv1a('@thumbnail');
  const coverHash = fnv1a('@cover');

  let coverEntry = null;
  let thumbEntry = null;
  let offset = 8;
  for (let i = 0; i < numEntries; i++) {
    if (offset + 20 > mediaData.length) break;
    const hash = view.getUint32(offset, true);
    const blobOffset = view.getUint32(offset + 4, true);
    const size = view.getUint32(offset + 8, true);
    const width = view.getUint32(offset + 12, true);
    const height = view.getUint32(offset + 16, true);

    if (hash === coverHash) {
      coverEntry = { offset: blobOffset, size, width, height };
    } else if (hash === thumbHash) {
      thumbEntry = { offset: blobOffset, size, width, height };
    }
    offset += 20;
  }

  const targetEntry = thumbEntry || coverEntry;
  if (!targetEntry || targetEntry.offset + targetEntry.size > mediaData.length) return null;
  return {
    width: targetEntry.width,
    height: targetEntry.height,
    data: mediaData.subarray(targetEntry.offset, targetEntry.offset + targetEntry.size)
  };
}

/** Render Floyd-Steinberg 1-bit raw bitmap to an HTML canvas */
function render1BitCoverToCanvas(canvas, coverObj) {
  if (!canvas || !coverObj) return;
  const { width, height, data } = coverObj;
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext('2d');
  const imgData = ctx.createImageData(width, height);
  const pixels = imgData.data;

  const paddedW = Math.ceil(width / 8) * 8;
  const widthBytes = paddedW / 8;

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const byteIdx = y * widthBytes + Math.floor(x / 8);
      const bitOffset = 7 - (x % 8);
      const isWhite = (data[byteIdx] & (1 << bitOffset)) !== 0;
      const val = isWhite ? 255 : 0;
      const pIdx = (y * width + x) * 4;
      pixels[pIdx] = val;
      pixels[pIdx + 1] = val;
      pixels[pIdx + 2] = val;
      pixels[pIdx + 3] = 255;
    }
  }
  ctx.putImageData(imgData, 0, 0);
}

/** Inspects and loads an unpacked or raw .eenk / .zip package into preloaded state */
async function loadPreloadedStoryPackage(buffer, originalFileName, rawBlobUrl = null) {
  try {
    setInspectProgress('Unpacking and verifying story package…', 30);
    const files = await unpackZip(buffer);
    const binEntry = files.find(f => f.name.toLowerCase().endsWith('.bin'));
    if (!binEntry) {
      throw new Error('No .bin story file found inside the package.');
    }

    const meta = parseStoryMetadata(binEntry.data);
    if (!meta) {
      throw new Error('Installation blocked: The package does not contain a valid compiled eenk story binary.');
    }

    const sidecars = files.filter(f => f !== binEntry && (f.name.endsWith('.media') || f.name.endsWith('.epdfont')));
    const mediaEntry = files.find(f => f.name.endsWith('.media'));

    let coverBitmap = null;
    if (mediaEntry) {
      coverBitmap = extractMediaCover(mediaEntry.data);
    }

    let coverImgUrl = null;
    if (!coverBitmap) {
      const imgEntry = files.find(f => /\.(png|jpg|jpeg|webp)$/i.test(f.name));
      if (imgEntry) {
        const mime = imgEntry.name.endsWith('.png') ? 'image/png' : 'image/jpeg';
        const blob = new Blob([imgEntry.data], { type: mime });
        coverImgUrl = URL.createObjectURL(blob);
      }
    }

    const folderName = (meta.title.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'story').substring(0, 32);

    setState({
      preloadedStory: {
        title: meta.title,
        author: meta.author,
        compileTime: meta.compileTime,
        fontName: meta.fontName,
        folderName,
        binData: binEntry.data,
        binName: binEntry.name,
        sidecars: sidecars.map(s => ({ name: s.name, data: s.data })),
        coverBitmap,
        coverImgUrl,
        rawBlobUrl,
        sourceFileName: originalFileName
      }
    });

    hideInspectProgress();
  } catch (err) {
    hideInspectProgress();
    showError(err.message);
  }
}

/** Loads a story directly from catalog.json without unzipping */
async function loadCatalogStory(slug) {
  try {
    setInspectProgress(`Loading "${slug}" from catalog…`, 20);
    let catalog = null;
    const catalogPaths = ['catalog.json', '../catalog.json', '../../catalog.json', '/catalog.json', 'docs/catalog.json'];
    for (const p of catalogPaths) {
      try {
        const res = await fetch(p);
        if (res.ok) {
          catalog = await res.json();
          break;
        }
      } catch (_) { }
    }
    if (!catalog || !catalog[slug]) {
      throw new Error(`Story "${slug}" not found in catalog.`);
    }

    const entry = catalog[slug];
    const files = Array.isArray(entry.files) ? entry.files : [entry.bin, entry.media, entry.font].filter(Boolean);
    if (!files || files.length === 0) {
      throw new Error(`"${entry.title || slug}" is featured as a showcase and is not hosted for 1-click install. Please visit the author's website.`);
    }

    setInspectProgress(`Downloading story files for "${entry.title || slug}"…`, 40);

    const fileEntries = [];

    for (let i = 0; i < files.length; i++) {
      const fileUrl = files[i];
      const res = await fetch(fileUrl);
      if (!res.ok) throw new Error(`Failed to download ${fileUrl} (HTTP ${res.status})`);
      const buf = await res.arrayBuffer();
      const filename = fileUrl.split(/[\\\/]/).pop();
      fileEntries.push({ name: filename, data: new Uint8Array(buf) });
    }

    const binEntry = fileEntries.find(f => f.name.toLowerCase().endsWith('.bin'));
    if (!binEntry) throw new Error('Catalog entry is missing a .bin story file.');

    const meta = parseStoryMetadata(binEntry.data);
    if (!meta) throw new Error('Installation blocked: Catalog story binary is invalid.');

    const sidecars = fileEntries.filter(f => f !== binEntry && (f.name.endsWith('.media') || f.name.endsWith('.epdfont')));
    const mediaEntry = fileEntries.find(f => f.name.endsWith('.media'));

    let coverBitmap = null;
    if (mediaEntry) {
      coverBitmap = extractMediaCover(mediaEntry.data);
    }
    let coverImgUrl = (!coverBitmap && (entry.thumbnail || entry.cover)) ? (entry.thumbnail || entry.cover) : null;

    const folderName = (meta.title.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || slug || 'story').substring(0, 32);

    setState({
      preloadedStory: {
        title: meta.title || entry.title,
        author: meta.author || entry.author,
        compileTime: meta.compileTime,
        fontName: meta.fontName,
        folderName,
        binData: binEntry.data,
        binName: binEntry.name,
        sidecars: sidecars.map(s => ({ name: s.name, data: s.data })),
        coverBitmap,
        coverImgUrl,
        rawBlobUrl: null,
        sourceFileName: `${slug}.eenk`
      }
    });

    hideInspectProgress();
  } catch (err) {
    hideInspectProgress();
    showError(`Catalog load error: ${err.message}`);
  }
}

/** Resolves CORS-restricted host URLs (e.g. GitHub blobs & raw links) into CORS-friendly CDN URLs */
function resolveCorsFriendlyUrl(urlStr) {
  try {
    const parsed = new URL(urlStr, window.location.href);

    // Case 1: https://github.com/<owner>/<repo>/blob/<ref>/<path...>
    // -> https://cdn.jsdelivr.net/gh/<owner>/<repo>@<ref>/<path...>
    if (parsed.hostname === 'github.com') {
      const match = parsed.pathname.match(/^\/([^\/]+)\/([^\/]+)\/blob\/([^\/]+)\/(.+)$/);
      if (match) {
        const [, owner, repo, ref, filePath] = match;
        return {
          url: `https://cdn.jsdelivr.net/gh/${owner}/${repo}@${ref}/${filePath}`,
          host: `github.com (${owner}/${repo})`
        };
      }
    }

    // Case 2: https://raw.githubusercontent.com/<owner>/<repo>/<ref>/<path...>
    // -> https://cdn.jsdelivr.net/gh/<owner>/<repo>@<ref>/<path...>
    if (parsed.hostname === 'raw.githubusercontent.com') {
      const match = parsed.pathname.match(/^\/([^\/]+)\/([^\/]+)\/([^\/]+)\/(.+)$/);
      if (match) {
        const [, owner, repo, ref, filePath] = match;
        return {
          url: `https://cdn.jsdelivr.net/gh/${owner}/${repo}@${ref}/${filePath}`,
          host: `github.com (${owner}/${repo})`
        };
      }
    }

    return { url: urlStr, host: parsed.hostname };
  } catch (_) {
    return { url: urlStr, host: urlStr };
  }
}

/** Handles external package URL with origin verification */
async function handleExternalPackageUrl(urlStr) {
  try {
    const resolved = resolveCorsFriendlyUrl(urlStr);
    const parsed = new URL(resolved.url, window.location.href);
    const isSameOrigin = parsed.origin === window.location.origin;

    if (!isSameOrigin) {
      setState({ pendingExternalUrl: resolved.url, externalHost: resolved.host });
      return;
    }

    await fetchAndInspectPackage(resolved.url);
  } catch (err) {
    showError(`Invalid package URL: ${err.message}`);
  }
}

async function fetchAndInspectPackage(urlStr) {
  try {
    const hostname = new URL(urlStr, window.location.href).hostname;
    setInspectProgress(`Downloading package from ${hostname}…`, 25);
    const res = await fetch(urlStr);
    if (!res.ok) {
      throw new Error(`Failed to download package (HTTP ${res.status})`);
    }
    const buf = await res.arrayBuffer();
    const filename = urlStr.split(/[\\\/]/).pop() || 'story.eenk';
    const blob = new Blob([buf], { type: 'application/octet-stream' });
    const rawBlobUrl = URL.createObjectURL(blob);
    await loadPreloadedStoryPackage(buf, filename, rawBlobUrl);
  } catch (err) {
    hideInspectProgress();
    showError(`Could not download story package: ${err.message}. If blocked by CORS, download the file directly and drop it here.`);
  }
}

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
    transferState: null, error: null, uploadSuccess: null,
    isPreloadUploading: false, isConfirming: false,
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

    if (path.startsWith('/books/') || name.toLowerCase().endsWith('.epub')) {
      const stem = name.replace(/\.[^/.]+$/, '');
      if (stem) {
        try {
          await protocol.deleteFile(`/.eenk_cache/${stem}`);
        } catch (_) { }
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

/** Upload a .bin and optional sidecars with continuous proportional progress. */
async function executeUpload(binFile, binData, sidecars) {
  if (!state.isConnected || !protocol) return;
  try {
    let title = binFile.name ? binFile.name.replace(/\.bin$/i, '') : 'story';
    const meta = parseStoryMetadata(binData);
    if (meta && meta.title) title = meta.title;

    const folderName = (title.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '') || 'story').substring(0, 32);

    try { await protocol.mkdir(`/stories/${folderName}`); } catch (_) { }

    // Pre-collect all batch files and resolve binary buffers
    const batch = [];
    const mainBinData = binData instanceof Uint8Array ? binData : new Uint8Array(binData);
    batch.push({
      name: binFile.name || 'story.bin',
      destPath: `/stories/${folderName}/story.bin`,
      data: mainBinData
    });

    for (const sidecar of (sidecars || [])) {
      let data = null;
      if (sidecar.data) {
        data = sidecar.data instanceof Uint8Array ? sidecar.data : new Uint8Array(sidecar.data);
      } else if (sidecar.arrayBuffer) {
        const buf = await sidecar.arrayBuffer();
        data = new Uint8Array(buf);
      }
      if (!data) continue;

      const destName = sidecar.name.endsWith('.media') ? 'story.media' : sidecar.name;
      batch.push({
        name: sidecar.name,
        destPath: `/stories/${folderName}/${destName}`,
        data
      });
    }

    const totalBatchBytes = batch.reduce((sum, item) => sum + item.data.length, 0);
    let completedBytes = 0;

    for (let i = 0; i < batch.length; i++) {
      const item = batch[i];
      const itemSize = item.data.length;
      const label = batch.length > 1
        ? `${item.name} (${i + 1}/${batch.length})`
        : item.name;

      setState({
        transferState: {
          type: 'upload',
          filename: label,
          bytesTransferred: completedBytes,
          bytesTotal: totalBatchBytes
        }
      });

      await protocol.uploadFile(item.destPath, item.data, (transferred, total) => {
        setState({
          transferState: {
            type: 'upload',
            filename: label,
            bytesTransferred: completedBytes + transferred,
            bytesTotal: totalBatchBytes
          }
        });
      });

      completedBytes += itemSize;
    }

    await refreshFiles();
    setState({ uploadSuccess: { title } });
  } catch (e) {
    showError('Upload failed: ' + e.message);
  } finally {
    clearTransfer();
    _pendingBin = null;
    _pendingSidecars = [];
    sidecarPrompt.style.display = 'none';
  }
}

/** Executes upload of currently preloaded story */
let _isPreloadUploading = false;
async function executePreloadedUpload() {
  if (!state.preloadedStory || _isPreloadUploading || state.transferState) return;
  _isPreloadUploading = true;
  setState({ isPreloadUploading: true });
  try {
    const { binName, binData, sidecars, title } = state.preloadedStory;
    await executeUpload({ name: binName }, binData, sidecars);
    dismissPreloadedStory();
    setState({ uploadSuccess: { title } });
  } finally {
    _isPreloadUploading = false;
    setState({ isPreloadUploading: false });
  }
}

/** Handle a .bin file selection — discover sidecars then decide next step. */
async function handleBinSelected(binFile, providedFiles = []) {
  const buf = await binFile.arrayBuffer();
  const binData = new Uint8Array(buf);
  _pendingBin = { file: binFile, data: binData };

  const meta = parseStoryMetadata(binData);
  if (!meta) {
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
          found.push({ name: entry, arrayBuffer: () => Promise.resolve(rawBuf.buffer ?? rawBuf) });
        }
      }
      await executeUpload(binFile, binData, found);
    } catch (e) {
      console.warn('[device-manager] Sidecar scan failed:', e);
      await executeUpload(binFile, binData, []);
    }
  } else {
    // Browser: check if sidecars were provided in the same drop/selection
    const SIDECAR_EXTS = ['.epdfont', '.media'];
    const baseName = binFile.name.replace(/\.bin$/i, '');

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
      const newFiles = Array.from(input.files || []);
      if (newFiles.length > 0) {
        const existingNames = new Set(_pendingSidecars.map(f => f.name));
        const uniqueNew = newFiles.filter(f => !existingNames.has(f.name));
        _pendingSidecars = [..._pendingSidecars, ...uniqueNew];
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
let _pendingConfirmCallback = null;

function showConfirmDialog({ title, messageHtml, okText = 'Confirm', okColor = '', cancelText = 'Cancel' }) {
  return new Promise((resolve) => {
    setState({ isConfirming: true });
    if (confirmHeading) confirmHeading.textContent = title || 'Confirm Action';
    confirmText.innerHTML = messageHtml;
    confirmOk.textContent = okText;
    confirmOk.style.background = okColor || '#CC0000';
    confirmOk.style.borderColor = okColor || '#CC0000';
    confirmCancel.textContent = cancelText;
    confirmModal.style.display = '';

    _pendingConfirmCallback = (result) => {
      confirmModal.style.display = 'none';
      _pendingConfirmCallback = null;
      // Reset styling
      if (confirmHeading) confirmHeading.textContent = 'Delete item?';
      confirmOk.textContent = 'Delete';
      confirmOk.style.background = '#CC0000';
      confirmOk.style.borderColor = '#CC0000';
      confirmCancel.textContent = 'Cancel';
      setState({ isConfirming: false });
      resolve(result);
    };
  });
}

async function promptDelete(path, name) {
  const ok = await showConfirmDialog({
    title: 'Delete item?',
    messageHtml: `Delete "${name}"? This cannot be undone.`,
    okText: 'Delete',
    okColor: '#CC0000'
  });
  if (ok) {
    await deleteItem(path, name);
  }
}

confirmOk.addEventListener('click', () => {
  if (_pendingConfirmCallback) _pendingConfirmCallback(true);
});

confirmCancel.addEventListener('click', () => {
  if (_pendingConfirmCallback) _pendingConfirmCallback(false);
});

/* ── Event wiring ───────────────────────────────────────────────── */
connectBtn.addEventListener('click', connect);
disconnectBtn.addEventListener('click', disconnect);
refreshBtn.addEventListener('click', refreshFiles);

// Preload & Consent Buttons
if (consentProceedBtn) {
  consentProceedBtn.addEventListener('click', async () => {
    if (state.pendingExternalUrl) {
      const url = state.pendingExternalUrl;
      setState({ pendingExternalUrl: null, externalHost: null });
      await fetchAndInspectPackage(url);
    }
  });
}

if (consentDismissBtn) {
  consentDismissBtn.addEventListener('click', () => {
    setState({ pendingExternalUrl: null, externalHost: null });
    cleanUrlParams();
  });
}

if (preloadInstallBtn) {
  preloadInstallBtn.addEventListener('click', async () => {
    if (_isPreloadUploading || state.transferState || state.isConnecting) return;
    const wasConnected = state.isConnected;
    if (!state.isConnected) {
      await connect();
    }

    if (!state.isConnected || !state.preloadedStory || _isPreloadUploading || state.transferState) return;

    const storyExists = state.stories.some(
      s => s.name === state.preloadedStory.folderName || s.path === `/stories/${state.preloadedStory.folderName}`
    );

    // If device was just connected and an existing story conflict was detected,
    // do NOT start uploading automatically. The save warning is now displayed;
    // wait for user to click "Update & Overwrite Story" to confirm.
    if (!wasConnected && storyExists) {
      return;
    }

    await executePreloadedUpload();
  });
}

if (preloadDismissBtn) {
  preloadDismissBtn.addEventListener('click', () => {
    dismissPreloadedStory();
  });
}

if (successDisconnectBtn) {
  successDisconnectBtn.addEventListener('click', async () => {
    setState({ uploadSuccess: null });
    await disconnect();
  });
}

if (successContinueBtn) {
  successContinueBtn.addEventListener('click', () => {
    setState({ uploadSuccess: null });
  });
}

// Tabs
document.querySelectorAll('.dm-tab').forEach(btn => {
  btn.addEventListener('click', () => {
    setState({ currentTab: btn.dataset.tab, uploadSuccess: null });
  });
});

async function executeBookUpload(epubFile) {
  return handleEpubSelected([epubFile]);
}

const LARGE_EPUB_THRESHOLD = 2 * 1024 * 1024; // 2 MB

async function handleEpubSelected(epubFiles) {
  if (!epubFiles || !epubFiles.length || !state.isConnected || !protocol) return;

  const validFiles = [];
  for (const epub of epubFiles) {
    if (epub.size && epub.size > LARGE_EPUB_THRESHOLD) {
      const proceed = await showConfirmDialog({
        title: 'Large EPUB Warning',
        messageHtml: `<strong>${epub.name}</strong> is ${formatSize(epub.size)} (&gt; 2 MB).<br><br>EPUB files over 2 MB frequently encounter read/transfer errors over USB serial, and transfer speeds are limited.<br><br><strong>Recommendation:</strong> Copy this book directly into the <code>/books</code> directory on your MicroSD card via an SD card reader instead.<br><br>Do you still want to attempt upload over USB?`,
        okText: 'Upload Anyway',
        okColor: '#e67e22',
        cancelText: 'Cancel'
      });
      if (!proceed) continue;
    }
    validFiles.push(epub);
  }

  if (validFiles.length === 0) return;

  try {
    try { await protocol.mkdir('/books'); } catch (_) { }

    const batch = [];
    for (const epub of validFiles) {
      const buf = await epub.arrayBuffer();
      batch.push({
        name: epub.name,
        destPath: `/books/${epub.name}`,
        data: new Uint8Array(buf)
      });
    }

    const totalBatchBytes = batch.reduce((sum, b) => sum + b.data.length, 0);
    let completedBytes = 0;

    for (let i = 0; i < batch.length; i++) {
      const item = batch[i];
      const itemSize = item.data.length;
      const label = batch.length > 1
        ? `${item.name} (${i + 1}/${batch.length})`
        : item.name;

      setState({
        transferState: {
          type: 'upload',
          filename: label,
          bytesTransferred: completedBytes,
          bytesTotal: totalBatchBytes
        }
      });

      await protocol.uploadFile(item.destPath, item.data, (transferred, total) => {
        setState({
          transferState: {
            type: 'upload',
            filename: label,
            bytesTransferred: completedBytes + transferred,
            bytesTotal: totalBatchBytes
          }
        });
      });

      completedBytes += itemSize;
    }

    await refreshFiles();
  } catch (e) {
    showError('Upload failed: ' + e.message);
  } finally {
    clearTransfer();
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

  const pkg = files.find(f => f.name.endsWith('.eenk') || f.name.endsWith('.zip'));
  if (pkg) {
    const buf = await pkg.arrayBuffer();
    await loadPreloadedStoryPackage(buf, pkg.name);
    return;
  }

  const bin = files.find(f => f.name.endsWith('.bin'));
  if (bin) await handleBinSelected(bin, files);
  else if (files.length > 0) showError("Please include a .eenk or .bin story file.");
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

  const pkg = files.find(f => f.name.endsWith('.eenk') || f.name.endsWith('.zip'));
  if (pkg) {
    const buf = await pkg.arrayBuffer();
    await loadPreloadedStoryPackage(buf, pkg.name);
    return;
  }

  const bin = files.find(f => f.name.endsWith('.bin'));
  if (bin) await handleBinSelected(bin, files);
  else if (files.length > 0) showError("Please include a .eenk or .bin story file.");
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

/* ── Query Parameter Handler & Startup ─────────────────────────── */
async function processQueryParams() {
  if (typeof window === 'undefined' || !window.location.search) return;
  const params = new URLSearchParams(window.location.search);

  if (params.has('story')) {
    const slug = params.get('story');
    if (slug) await loadCatalogStory(slug);
  } else if (params.has('url')) {
    const url = params.get('url');
    if (url) await handleExternalPackageUrl(url);
  }
}

/* ── Web Serial availability check & init ──────────────────────── */
(function init() {
  if (!('serial' in navigator)) {
    $('serial-warn').style.display = '';
    connectBtn.disabled = true;
  }
  render();
  processQueryParams();
})();
