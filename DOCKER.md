# Запуск на Linux-сервере без Docker Hub / регистраций

Образ собран и лежит **готовым архивом** (никакого аккаунта Docker Hub не нужно —
перекачиваешь архив, делаешь `docker load`, запускаешь). Поддерживает две архитектуры:
- `dist/polymarket-hft-amd64.tar.gz` — большинство VPS (Intel/AMD).
- `dist/polymarket-hft-arm64.tar.gz` — ARM (Apple Silicon VM, Ampere, Graviton и т.п.).

Внутри один контейнер: **движок (HTTP 8080 + IPC 9999) + live feed bridge + дашборд (3000)**.

---

## 1. Передать образ на сервер

Со своей машины (amd64-вариант):

```bash
scp dist/polymarket-hft-amd64.tar.gz user@YOUR_SERVER:/tmp/
# или на arm-сервер:
scp dist/polymarket-hft-arm64.tar.gz user@YOUR_SERVER:/tmp/
```

## 2. Загрузить образ и запустить на сервере

```bash
# на сервере
gunzip -c /tmp/polymarket-hft-amd64.tar.gz | docker load   # -> polymarket-hft:latest

mkdir -p ~/pmhft && cd ~/pmhft
# (если есть docker-compose.yml — клади его рядом с проектом,
#  либо просто docker run):

docker run -d --name polymarket-hft --restart unless-stopped \
  -p 8080:8080 -p 3000:3000 \
  -v polymarket-data:/app/data \
  polymarket-hft:latest
```

или через compose:

```bash
docker compose up -d
```

## 3. Проверка

```bash
docker logs -f polymarket-hft          # логи движка/фида/дашборда
curl -s http://localhost:8080/api/status   # статус движка (PAPER, btc_price, has_live_market)
# дашборд: http://SERVER:3000
```

Ожидается в статусе: `"mode":"PAPER"`, живой `btc_price`, и после того как спустит
внешний сетевой блок на Polymarket — `has_live_market:1` и `market_slug` = открытое
5-мин окно `btc-updown-5m-<epoch>`.

## Альтернатива: собрать прямо на сервере (не нужен архив)

Если на сервере есть git + интернет:

```bash
git clone <репо> && cd polymarket
docker build -t polymarket-hft:latest .   # или docker compose build
docker compose up -d
```

---

### Параметры (опционально)
| ENV | default | смысл |
|---|---|---|
| `ENGINE_PORT` | `8080` | HTTP-порт движка |
| `DASH_PORT` | `3000`  | порт дашборда |
| volume `/app/data` | — | персистентные датасет/логи/статистика |

> PAPER-режим жёстко залочен (`LIVE_TRADING=true` вызывает FATAL в движке) —
> настоящие ключи не задействуются и live-торговля не включается в контейнере.