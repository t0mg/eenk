// We use window.require to grab Electron's ipcRenderer, which bypasses Vite's bundler.
const electron = window.require ? window.require('electron') : null;
const ipcRenderer = electron ? electron.ipcRenderer : null;
const webUtils = electron ? electron.webUtils : null;

const dropZone = document.getElementById('drop-zone');
const logContainer = document.getElementById('log-container');
const logOutput = document.getElementById('log-output');
const statusIndicator = document.getElementById('status-indicator');

// Handle Drag and Drop
['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
  dropZone.addEventListener(eventName, preventDefaults, false);
});

function preventDefaults(e) {
  e.preventDefault();
  e.stopPropagation();
}

['dragenter', 'dragover'].forEach(eventName => {
  dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'), false);
});

['dragleave', 'drop'].forEach(eventName => {
  dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'), false);
});

dropZone.addEventListener('drop', handleDrop, false);

function handleDrop(e) {
  const dt = e.dataTransfer;
  const files = dt.files;

  if (files.length > 0) {
    const file = files[0];
    if (file.name.endsWith('.ink')) {
      const filePath = webUtils ? webUtils.getPathForFile(file) : file.path;
      startCompilation(filePath);
    } else {
      appendLog('[ERROR] Please drop a valid .ink file', 'error');
    }
  }
}

// Click to select file (fallback for drag and drop)
dropZone.addEventListener('click', () => {
  appendLog('[INFO] Drag and drop is supported! Please drag an .ink file here.', 'processing');
});

function appendLog(msg, statusClass = null) {
  logOutput.textContent += msg + '\n';
  logOutput.scrollTop = logOutput.scrollHeight;
  
  if (statusClass) {
    statusIndicator.className = `status-indicator ${statusClass}`;
    if (statusClass === 'success') statusIndicator.textContent = 'Success';
    else if (statusClass === 'error') statusIndicator.textContent = 'Error';
    else if (statusClass === 'processing') statusIndicator.textContent = 'Processing...';
  }
}

async function startCompilation(filePath) {
  logOutput.textContent = ''; // clear logs
  logContainer.classList.remove('hidden');
  appendLog(`[INFO] Received file: ${filePath}`, 'processing');
  
  if (!ipcRenderer) {
    appendLog('[ERROR] IPC not found! Are you running this via Electron?', 'error');
    return;
  }

  try {
    await ipcRenderer.invoke('compile-story', filePath);
    appendLog(`[SUCCESS] Story compiled successfully!`, 'success');
  } catch (err) {
    appendLog(`[ERROR] Compilation failed: ${err}`, 'error');
  }
}

if (ipcRenderer) {
  ipcRenderer.on('compile-progress', (event, msg) => {
    appendLog(msg);
  });
}
