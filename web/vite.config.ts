import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';

// The development server, with hot reload, on port 5174: the camera's container
// shares the host's network, where the StepIt Editor's takes 5173. The pictures
// come from the web server of the camera, on port 8090, as when it serves the
// page.
export default defineConfig({
  plugins: [react()],
  server: {
    host: '0.0.0.0',
    port: 5174,
    strictPort: true,
    proxy: { '/pictures': 'http://localhost:8090' },
  },
  build: { outDir: 'dist', emptyOutDir: true },
});
