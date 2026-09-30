// The production server: the UI built by `vite build`, as static files. The UI
// needs no API of its own: the browser talks to the robot directly, through
// rosbridge and web_video_server.

import { createReadStream, existsSync, statSync } from 'node:fs';
import { createServer } from 'node:http';
import { dirname, extname, join, normalize, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const DIST = resolve(dirname(fileURLToPath(import.meta.url)), '../../dist');
const PORT = Number(process.env.PORT ?? 8080);
const HOST = process.env.HOST ?? '0.0.0.0';

const TYPES: Record<string, string> = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript',
  '.css': 'text/css',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.ico': 'image/x-icon',
  '.json': 'application/json',
};

if (!existsSync(join(DIST, 'index.html'))) {
  console.error(`The UI is not built: ${DIST}/index.html is missing. Run build.sh first.`);
  process.exit(1);
}

const server = createServer((req, res) => {
  const url = new URL(req.url ?? '/', 'http://localhost');
  let file = normalize(join(DIST, decodeURIComponent(url.pathname)));
  if (!file.startsWith(DIST + sep) || !existsSync(file) || statSync(file).isDirectory()) {
    file = join(DIST, 'index.html');
  }
  res.setHeader('Content-Type', TYPES[extname(file)] ?? 'application/octet-stream');
  createReadStream(file).pipe(res);
}).listen(PORT, HOST, () => {
  // In the container, UI_PORT is the host port that docker publishes as the
  // container's port 8080: any other port is unreachable from the host.
  const published = process.env.UI_PORT;
  if (!published) {
    console.log(`StepIt UI on http://localhost:${PORT}`);
  } else if (PORT === 8080) {
    console.log(`StepIt UI on http://localhost:${published}`);
  } else {
    console.warn(`Listening on port ${PORT} of the container, which docker does not publish.`);
    console.warn('To change the port, start the container with UI_PORT=<port> ./docker/dock.sh ...');
  }
});

server.on('error', (e: NodeJS.ErrnoException) => {
  if (e.code !== 'EADDRINUSE') throw e;
  console.error(`Port ${PORT} is already in use. Choose another one with PORT=<port>.`);
  process.exit(1);
});
