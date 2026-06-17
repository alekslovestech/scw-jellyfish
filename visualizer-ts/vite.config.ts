import { defineConfig } from 'vite';
import { resolve } from 'path';

function wifiRedirectMiddleware() {
  return {
    name: 'wifi-redirect',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url === '/wifi' || req.url === '/wifi/') {
          res.statusCode = 302;
          res.setHeader('Location', '/wifi/index.html');
          res.end();
          return;
        }
        next();
      });
    },
    configurePreviewServer(server) {
      server.middlewares.use((req, res, next) => {
        if (req.url === '/wifi' || req.url === '/wifi/') {
          res.statusCode = 302;
          res.setHeader('Location', '/wifi/index.html');
          res.end();
          return;
        }
        next();
      });
    },
  };
}

export default defineConfig({
  build: {
    chunkSizeWarningLimit: 800,
    rollupOptions: {
      input: {
        main: resolve(__dirname, 'index.html'),
        wifi: resolve(__dirname, 'wifi/index.html'),
      },
    },
  },
  plugins: [wifiRedirectMiddleware()],
});
