# Diggy HTTP Server

Extremely minimalistic, static-route HTTP server with simple config via file, env, or CLI.  
Docker image size less than 100kB.

## Quick Start

```bash
# Build & run with the provided Dockerfile
docker build -t diggy .
docker run --rm -p 8080:8080 diggy
```

### Image Labels

The Docker image includes metadata labels for size information:
- `image.size`: "small" 
- `image.size.description`: Description of the minimalistic nature
- `image.size.target`: "<100kB" target size
- Standard OCI labels for title, description, vendor, and source

To inspect the image labels after building:
```bash
./inspect_labels.sh diggy
```

## Binary Usage

```bash
./app -port=8080 -host=0.0.0.0 -interval_ms=2000 -poll_timeout_ms=50 -mine=1
```
or

```bash
DIGGY_HOST=0.0.0.0 DIGGY_PORT=8080 DIGGY_INTERVAL_MS=2000 DIGGY_POLL_TIMEOUT_MS=50 DIGGY_MINE=1 ./app
```
or

```bash
# Create a config file `diggy.conf` in the same directory as the binary
./app
```
or mix configs.

## Configuration priorities

Settings are loaded in this order (later overrides earlier):

1. `diggy.conf` (same directory as the binary)
2. Environment variables `DIGGY_*`
3. CLI flags `-key=value`

**Precedence:** CLI > Env > Config file

## Parameters

| Parameter | Keys (file / env / CLI) | Default | Effect |
|-----------|-------------------------|---------|--------|
| `port` | `port` / `DIGGY_PORT` / `-port=` | `8080` | TCP port to bind |
| `host` | `host` / `DIGGY_HOST` / `-host=` | `0.0.0.0` | IPv4 address to bind |
| `interval_ms` | `interval_ms` / `DIGGY_INTERVAL_MS` / `-interval_ms=` | `2000` | How often one line of built-in content is printed to stdout when mine=1. Cadence is tied to poll_timeout_ms |
| `poll_timeout_ms` | `poll_timeout_ms` / `DIGGY_POLL_TIMEOUT_MS` / `-poll_timeout_ms=` | `100` | Poll timeout per loop. Lower = more responsive & tighter timers (more CPU); higher = less CPU, coarser timers |
| `mine` | `mine` / `DIGGY_MINE` / `-mine=` | `1` | 1 enables periodic stdout "mining"; 0 disables. No effect on HTTP responses |

### Example Config File (`diggy.conf`)

```
port=8080
host=0.0.0.0
interval_ms=2000
poll_timeout_ms=100
mine=1
```


## Routes

All responses are `text/plain; charset=utf-8`.

- `GET /` – static content, song of the miners
- `GET /health` – OK
- `GET /about` – short server description
- `GET /info` – version/info
- Any other path → 404 Not Found

## Behavior Summary

- Binds to `host:port` and serves the fixed routes above
- Main loop polls the listening socket; every `interval_ms` (approx., based on `poll_timeout_ms`) prints the next line of the built-in content to stdout if `mine=1`
