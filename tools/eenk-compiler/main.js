import { app, BrowserWindow, ipcMain } from 'electron';
import path from 'path';
import { fileURLToPath } from 'url';
import { compileStory } from './compiler.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Detect CLI mode
const isCommandLineMode = process.argv.length > 1 && process.argv.some(arg => arg.endsWith('.ink'));

if (isCommandLineMode) {
  // --- HEADLESS CLI MODE ---
  const inkFile = process.argv.find(arg => arg.endsWith('.ink'));
  
  app.on('ready', async () => {
    console.log(`[CLI] Starting compilation for: ${inkFile}`);
    try {
      await compileStory(inkFile, (msg) => console.log(msg));
      console.log('[CLI] Compilation finished successfully.');
      app.quit();
    } catch (err) {
      console.error('[CLI] Compilation failed:', err);
      process.exit(1);
    }
  });

} else {
  // --- GUI MODE ---
  function createWindow() {
    const win = new BrowserWindow({
      width: 800,
      height: 600,
      title: "EENK Story Compiler",
      autoHideMenuBar: true,
      webPreferences: {
        nodeIntegration: true,
        contextIsolation: false
      }
    });

    // In dev mode, load the Vite dev server. In production, load the built HTML.
    if (process.env.VITE_DEV_SERVER_URL) {
      win.loadURL(process.env.VITE_DEV_SERVER_URL);
    } else {
      win.loadFile(path.join(__dirname, 'dist', 'index.html'));
    }
  }

  app.whenReady().then(() => {
    createWindow();

    app.on('activate', () => {
      if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
      }
    });
  });

  app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
      app.quit();
    }
  });

  // Handle IPC calls from the UI
  ipcMain.handle('compile-story', async (event, inkFile) => {
    return new Promise((resolve, reject) => {
      compileStory(inkFile, (msg) => {
        // Send progress updates back to the UI
        event.sender.send('compile-progress', msg);
      })
      .then(() => resolve(true))
      .catch((err) => reject(err.message || err));
    });
  });
}
