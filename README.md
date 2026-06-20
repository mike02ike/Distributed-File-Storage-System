# Distributed File Storage System
 
A distributed file storage backend built in C++ using low-level POSIX sockets. Inspired by systems like AWS S3 and Google Drive, this project progressively evolves from a simple TCP file transfer into a distributed, fault-tolerant storage system.
 
---
 
## Architecture
 
```
Client (Mac) ──TCP──► Server (Raspberry Pi)
                         └── storage/
```
 
Files are split into chunks on the client, transferred over TCP with CRC32 integrity verification, and reassembled on the server.
 
---
 
## Features (Current)
 
- TCP client/server with multi-client support via threads
- Binary protocol with length-prefixed fields
- Chunked file transfer (4MB chunks)
- CRC32 checksum verification per chunk
- Dedicated `storage/` directory on the server
- Graceful shutdown via SIGINT (Ctrl+C)
- Command-line IP address for client
---
 
## Protocol
 
### Header (sent once per transfer)
| Field | Type | Size |
|-------|------|------|
| Path length | uint32_t | 4 bytes |
| File path | char[] | path length bytes |
| File size | uint32_t | 4 bytes |
| Chunk count | uint32_t | 4 bytes |
 
### Per chunk (repeated for each chunk)
| Field | Type | Size |
|-------|------|------|
| Chunk index | uint32_t | 4 bytes |
| Chunk size | uint32_t | 4 bytes |
| Chunk data | bytes | chunk size bytes |
| CRC32 checksum | uint32_t | 4 bytes |
 
All multi-byte integers are sent in network byte order (big-endian).
 
---
 
## Project Structure
 
```
Distributed-File-Storage-System/
├── client/
│   ├── client.cpp
│   ├── client.h
│   └── Makefile
├── server/
│   ├── server.cpp
│   ├── server.h
│   ├── storage/        # received files (gitignored)
│   └── Makefile
└── common.h            # shared constants (PORT, CHUNK_SIZE)
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
 
**Server (Raspberry Pi):**
```bash
./server
```
 
**Client (Mac):**
```bash
./client <server_ip>
# Example:
./client 192.168.0.201
```
 
Then enter the file path when prompted.
 
---
 
## Roadmap
 
| Phase | Status | Description |
|-------|--------|-------------|
| 1 | ✅ Complete | Single server file transfer |
| 2 | ✅ Complete | File chunking with CRC32 verification |
| 3 | 🔄 In progress | Multi-server chunk distribution (round-robin) |
| 4 | ⬜ Planned | Replication — store multiple copies |
| 5 | ⬜ Planned | Fault tolerance — detect and recover from server failures |
| 6 | ⬜ Planned | Metadata coordinator — track file/chunk locations |
 
---
 
## Notes
 
- Received files are saved to `server/storage/` and are gitignored
- The server binds to all interfaces (`INADDR_ANY`) on port 8080 by default
- `SO_REUSEADDR` is set so the port can be reused immediately after shutdown
 