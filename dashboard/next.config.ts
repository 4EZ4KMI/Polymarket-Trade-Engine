import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  // Standalone output so the Docker runtime can run the dashboard with
  // `node server.js` without shipping node_modules/source.
  output: "standalone",
  reactStrictMode: true,
};

export default nextConfig;