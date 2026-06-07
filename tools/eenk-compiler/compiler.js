import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function getBinariesDir() {
  // In production, binaries are copied to resources/bin via extraResources.
  // We can robustly detect if we are packaged by checking if __dirname is inside app.asar.
  if (__dirname.includes('app.asar')) {
    return path.join(process.resourcesPath, 'bin', 'win');
  } else {
    return path.join(__dirname, 'bin', 'win');
  }
}

function runProcess(exePath, args, onProgress) {
  return new Promise((resolve, reject) => {
    const proc = spawn(exePath, args, { cwd: path.dirname(args[0]) });

    proc.stdout.on('data', (data) => {
      const msg = data.toString().trim();
      if (msg) onProgress(`[STDOUT] ${msg}`);
    });

    proc.stderr.on('data', (data) => {
      const msg = data.toString().trim();
      if (msg) onProgress(`[STDERR] ${msg}`);
    });

    proc.on('close', (code) => {
      if (code === 0) {
        resolve();
      } else {
        reject(new Error(`Process ${path.basename(exePath)} exited with code ${code}`));
      }
    });

    proc.on('error', (err) => {
      reject(err);
    });
  });
}

export async function compileStory(inkFileParam, onProgress) {
  const binDir = getBinariesDir();
  const inkFile = path.resolve(inkFileParam);
  
  // For MVP, we use Windows executables. Mac binaries would be selected based on process.platform here.
  const inklecateExe = path.join(binDir, 'inklecate.exe');
  const inkcppExe = path.join(binDir, 'inkcpp_cl.exe');

  if (!fs.existsSync(inkFile)) {
    throw new Error(`File not found: ${inkFile}`);
  }

  const inkDir = path.dirname(inkFile);
  const baseName = path.basename(inkFile, '.ink');
  const jsonFile = path.join(inkDir, `${baseName}.json`);

  onProgress(`--- Starting compilation for ${baseName}.ink ---`);
  
  onProgress(`Step 1: Running inklecate...`);
  await runProcess(inklecateExe, ['-o', jsonFile, inkFile], onProgress);
  onProgress(`inklecate generated: ${jsonFile}`);

  onProgress(`Step 2: Running inkcpp_cl...`);
  await runProcess(inkcppExe, [jsonFile], onProgress);
  
  const binFile = path.join(inkDir, `${baseName}.bin`);
  onProgress(`inkcpp_cl generated: ${binFile}`);
  
  onProgress(`--- Compilation finished successfully ---`);
}
