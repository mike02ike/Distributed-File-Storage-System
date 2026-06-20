# Distributed File Storage System

A distributed file storage backend built in C++ using low-level POSIX sockets. Inspired by systems like AWS S3 and Google Drive, this project progressively evolves from a simple TCP file transfer into a distributed, fault-tolerant storage system.

---

## Architecture

```
                    ┌──TCP──► Server (port 8080) ── storage/8080/
Client (Mac) ───────┼──TCP──► Server (port 8081) ── storage/8081/
                    └──TCP──► Server (port 8082) ── storage/8082/
```

Files are split into chunks on the client, distributed round-robin across multiple servers over TCP, and verified with CRC32 checksums on receipt.

---

## Features (Current)

- TCP client/server with multi-client support via threads
- Binary protocol with length-prefixed fields
- Chunked file transfer (4MB chunks)
- CRC32 checksum verification per chunk
- Multi-server round-robin chunk distribution
- Per-port storage directories on each server instance
- Configurable server list via `serverList.txt`
- Graceful shutdown via SIGINT (Ctrl+C)
- Configurable server port via command-line argument

---

## Protocol

### Header (sent once per transfer, to each server)
| Field | Type | Size |
|-------|------|------|
| Path length | uint32_t | 4 bytes |
| File path | char[] | path length bytes |
| File size | uint32_t | 4 bytes |
| Chunk count | uint32_t | 4 bytes (chunks assigned to *this* server) |

### Per chunk (repeated for each chunk)
| Field | Type | Size |
|-------|------|------|
| Chunk index | uint32_t | 4 bytes |
| Chunk size | uint32_t | 4 bytes |
| Chunk data | bytes | chunk size bytes |
| CRC32 checksum | uint32_t | 4 bytes |

All multi-byte integers are sent in network byte order (big-endian).

Chunks are distributed round-robin across servers (`chunkIndex % serverCount`), so each server only knows about and receives the chunks assigned to it.

---

## Project Structure

```
Distributed-File-Storage-System/
├── client/
│   ├── client.cpp
│   ├── client.h
│   ├── serverList.txt  # ip:port per line (gitignored)
│   └── Makefile
├── server/
│   ├── server.cpp
│   ├── server.h
│   ├── storage/         # received files, organized by port (gitignored)
│   │   ├── 8080/
│   │   ├── 8081/
│   │   └── 8082/
│   └── Makefile
└── common.h              # shared constants and helpers (CHUNK_SIZE, Server struct, formatBytes)
```

---

## Dependencies

- **zlib** — CRC32 checksum computation
  - Mac: `brew install zlib`
  - Linux/Pi: `sudo apt install zlib1g-dev`
- C++17 or later
- POSIX-compatible OS (macOS, Linux)

---

## Building

```bash
# Build server (from server/)
make server

# Build client (from client/)
make client

# Clean
make clean
```

---

## Usage

**Server (Raspberry Pi)** — run one instance per port:
```bash
./server 8080
./server 8081
./server 8082
```

**Client (Mac)** — create a `serverList.txt` in the client directory listing all available servers:
```
100.122.233.18:8080
100.122.233.18:8081
100.122.233.18:8082
```

Then run:
```bash
./client
```

Enter the file path when prompted. Chunks are distributed round-robin across all listed servers.

---

## Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ Complete | Single server file transfer |
| 2 | ✅ Complete | File chunking with CRC32 verification |
| 3 | ✅ Complete | Multi-server chunk distribution (round-robin) |
| 4 | ⬜ Planned | Replication — store multiple copies |
| 5 | ⬜ Planned | Fault tolerance — detect and recover from server failures |
| 6 | ⬜ Planned | Metadata coordinator — track file/chunk locations |

---

## Notes

- Received files are saved to `server/storage/<port>/` and are gitignored
- Each server binds to all interfaces (`INADDR_ANY`) on its specified port
- `SO_REUSEADDR` is set so a port can be reused immediately after shutdown
- If a server in `serverList.txt` is unreachable, the entire transfer currently fails — fault tolerance for partial server availability is planned for Phase 5
- Servers with zero assigned chunks (more servers than chunks) skip file creation entirely
