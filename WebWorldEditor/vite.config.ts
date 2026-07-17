import { defineConfig } from "vite";

// Static SPA. No server-side code — everything (decrypt + parse + render)
// runs in the browser, so this deploys as pure static output on Vercel.
export default defineConfig({
  base: "./",
  build: {
    target: "es2020",
    outDir: "dist",
    sourcemap: false
  }
});
