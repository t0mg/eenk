import { defineConfig } from 'vite';

export default defineConfig({
  base: './', // Ensures relative paths for Electron file:// protocol
});
