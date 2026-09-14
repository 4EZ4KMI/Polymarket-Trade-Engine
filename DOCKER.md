# Деплой на Linux-сервере (Docker, без Docker Hub)

Весь стек упакован в **один контейнер**: C-движок (HTTP `:8080`, IPC `:9999`) + live feed
bridge (Binance + Polymarket) + Next.js-дашборд (`:3000`). Docker-исходники
(`Dockerfile`, `docker/entrypoint.sh`, `docker-compose.yml`) закоммичены в

> **репозиторий:** `github.com/4EZ4KMI/Polymarket-Trade-Engine`

Есть **два способа** получить образ; реестр (Docker Hub) не требуется вообще.

---

## Способ A — собрать прямо на сервере (рекомендуется)

На сервере нужны git + интернет:

```bash
git clone https://github.com/4EZ4KMI/Polymarket-Trade-Engine.git polymarket
cd polymarket
docker compose up -d --build            # сборка + запуск, image: polymarket-hft:latest
```

Или без compose:

```bash
docker build -t polymarket-hft:latest .
docker run -d --name polymarket-hft --restart unless-stopped \
  -p 8080:8080 -p 3000:3000 \
  -v polymarket-data:/app/data \
  polymarket-hft:latest
```

> Примечание: в repo не хранятся образы. Внутри контейнера идёт сборка C-движка (gcc,
> epoll/Linux) и Next.js standalone — поэтому образ самостоятельный и работает **без**
> исходников на целевой машине после `docker load`.

---

## Способ B — готовый образ архивом (без интернета на сервере)

`dist/*.tar.gz` — это **локальные артефакты** (в git НЕ залиты: >100MB, лимит GitHub).
Собираются локально и переносятся на сервер через `scp`/USB:

### 1. Собрать архивы на этой машине
```bash
mkdir -p dist
docker build --platform linux/amd64 -t polymarket-hft:latest .
docker save polymarket-hft:latest | gzip > dist/polymarket-hft-amd64.tar.gz
# (для ARM — замените --platform на linux/arm64, тег polymarket-hft:latest-arm64)
```
Либо уже готовые образы в этой машинной среде: `polymarket-hft:amd64check` / `:buildcheck`.

### 2. Передать на сервер
```bash
scp dist/polymarket-hft-amd64.tar.gz user@YOUR_SERVER:/tmp/   # amd64 (Intel/AMD VPS)
# или
scp dist/polymarket-hft-arm64.tar.gz user@YOUR_SERVER:/tmp/   # ARM (Graviton, Ampere…)
```

### 3. Загрузить и запустить на сервере
```bash
gunzip -c /tmp/polymarket-hft-amd64.tar.gz | docker load        # -> polymarket-hft:latest
docker run -d --name polymarket-hft --restart unless-stopped \
  -p 8080:8080 -p 3000:3000 \
  -v polymarket-data:/app/data \
  polymarket-hft:latest
```

### Мульти-арч (linux/amd64+linux/arm64) в один образ
Без реестра — либо собрать архив в `/tmp` (OCI layout, грузится через `docker load`):
```bash
docker buildx build --platform linux/amd64,linux/arm64 \
  --output type=docker,dest=/tmp/polymarket-hft.tar .
```
Когда появится аккаунт регистра — пуш как обычно:
```bash
docker buildx build --platform linux/amd64,linux/arm64 \
  -t docker.io/<username>/polymarket-hft:latest --push .
```

---

## Проверка после запуска

```bash
docker logs -f polymarket-hft                     # логи движка/фида/дашборда
curl -s http://localhost:8080/api/status          # (mode=PAPER, btc_price, has_live_market)
# дашборд: http://<SERVER>:3000
```

Ожидается в `/api/status`: `"mode":"PAPER"`, живой `btc_price` (Binance), и после того как
сойдёт внешний сетевой блок на Polymarket — `has_live_market:1`, а `market_slug` =
открытое 5-мин окно `btc-updown-5m-<epoch>`.

---

## Параметры (опционально)
| ENV | default | смысл |
|---|---|---|
| `ENGINE_PORT` | `8080` | HTTP-порт движка |
| `DASH_PORT`   | `3000` | порт дашборда |
| volume `/app/data` | — | персистентные датасет/лог/статистика |

## Безопасность
- **Paper-only lock**: `LIVE_TRADING=true` вызывает `[FATAL SAFETY] ... permanently locked`
  — в контейнере live-торговля не включается, реальные ключи не задействуются.
- Порт `:8080` в контейнере слушает `0.0.0.0`; наружу маппь только если нужен
  публичный доступ, лучше за reverse-proxy/HTTPS.