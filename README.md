# Concurrent HTTP/1.1 Server in C

A multithreaded HTTP/1.1 static file server built from scratch in C11. Features a lock-free work queue, adaptive thread pool, and persistent connection handling.

## Features

| Feature                  | Implementation                                          |
| ------------------------ | ------------------------------------------------------- |
| **Lock-Free Queue**      | C11 atomics, single-producer multi-consumer ring buffer |
| **Adaptive Thread Pool** | Scales workers based on queue wait time (EMA)           |
| **HTTP/1.1 Keep-Alive**  | Persistent connections with configurable timeout        |
| **Zero-Copy I/O**        | `sendfile()` for static file serving                    |
| **Security**             | Path traversal protection (`../` sanitization)          |
| **Graceful Shutdown**    | SIGINT/SIGTERM handling with connection draining        |

## Architecture

```text
[Main Thread] ──accept()──▶ [Lock-Free Queue] ◄──dequeue── [Worker Threads]
│ (SPSC/MPMC) │
│ │
└──enqueue() └──process HTTP
│
▼
[Adaptive Pool Monitor]
(scale on wait time > 50ms)
```

## Build

```bash
make
./server -p 8080 -t 4 -m 16 -d ./www
```

## Usage

```bash
./server [options]

-p PORT Port to bind (default: 8080)
-t THREADS Initial thread pool size (default: 4)
-m MAX Maximum threads for adaptive scaling (default: 64)
-d ROOT Document root directory (default: ./www)
```

## Example

```bash
# Start server
mkdir -p www && echo "<h1>Hello</h1>" > www/index.html
./server -p 8080 -t 4 -m 16 -d ./www

# Terminal 2: Test
curl http://localhost:8080/index.html
curl -v http://localhost:8080/index.html http://localhost:8080/index.html # Keep-Alive

# Load test
wrk -t4 -c100 -d30s http://localhost:8080/index.html

```

## Specification Compliance

- [x] HTTP/1.1 persistent connections (Keep-Alive)
- [x] GET and HEAD methods
- [x] Host, Connection, Content-Length headers
- [x] Path traversal protection
- [x] Graceful shutdown
- [ ] Chunked transfer encoding (future work)
- [ ] Event-driven I/O reactor (future work)
