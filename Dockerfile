# =============================================================================
# Polymarket HFT Engine + Real Feed Bridge + Monitoring Dashboard — Container
# Single container runs all three (engine on 8080, dashboard on 3000, IPC 9999)
# because the dashboard reads the engine API at hardcoded localhost:8080.
# Multi-arch: linux/amd64 + linux/arm64 via buildx/QEMU.
# =============================================================================

# ---- Stage 1: build the C11 engine & replay tools (epoll/Linux) -------------
FROM gcc:13-bookworm AS engine-build
WORKDIR /src
COPY Makefile ./
COPY include/ include/
COPY src/ src/
# Build the core library, engine and replay binaries. Tests are skipped at image
# build time; the engine/replay are what the runtime needs.
RUN make engine replay && \
    ls -la build/bin/

# ---- Stage 2: build the Next.js monitoring dashboard (standalone output) ----
FROM node:20-bookworm-slim AS dash-build
WORKDIR /dash
COPY dashboard/package.json dashboard/package-lock.json ./
RUN npm ci --no-audit --no-fund
COPY dashboard/ ./
RUN npm run build && \
    ls -la .next/standalone && \
    ls -la .next/

# ---- Runtime: node + python3 + glibc runtime --------------------------------
FROM node:20-bookworm-slim AS runtime
ENV PYTHONUNBUFFERED=1 \
    ENGINE_PORT=8080 \
    DASH_PORT=3000 \
    NODE_ENV=production
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 python3-websockets ca-certificates curl procps && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# C engine binaries (engine + replay)
COPY --from=engine-build /src/build/bin/ build/bin/

# Dashboard standalone runtime + static assets
COPY --from=dash-build /dash/.next/standalone/ /app/dashboard/
COPY --from=dash-build /dash/.next/static/ /app/dashboard/.next/static/

# Config + live feed bridge + entrypoint
COPY config/ config/
COPY scripts/ scripts/
COPY README.md ./
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Persistent live data (dataset, logs, strategy stats)
VOLUME ["/app/data"]

EXPOSE 8080 3000 9999
HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD curl -fsS http://127.0.0.1:8080/api/status >/dev/null || exit 1

CMD ["/usr/local/bin/entrypoint.sh"]