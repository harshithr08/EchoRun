import fs from "node:fs";
import path from "node:path";

import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

function serveRepoOutDirectory() {
  const outDir = path.resolve(__dirname, "../out");

  return {
    name: "serve-repo-out-directory",
    configureServer(server) {
      server.middlewares.use("/out", (req, res, next) => {
        const requestPath = decodeURIComponent(req.url || "/");
        const cleanPath = requestPath.replace(/^\/+/, "");
        const filePath = path.join(outDir, cleanPath);

        if (!filePath.startsWith(outDir) || !fs.existsSync(filePath) || fs.statSync(filePath).isDirectory()) {
          next();
          return;
        }

        if (filePath.endsWith(".json")) res.setHeader("Content-Type", "application/json; charset=utf-8");
        if (filePath.endsWith(".txt")) res.setHeader("Content-Type", "text/plain; charset=utf-8");
        res.end(fs.readFileSync(filePath));
      });
    },
  };
}

export default defineConfig({
  plugins: [react(), serveRepoOutDirectory()],
  server: {
    fs: {
      allow: [path.resolve(__dirname, "..")],
    },
  },
});
