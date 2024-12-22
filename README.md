# 🚀 High-Performance Distributed In-Memory Data Store

This repository contains a highly performant, low-latency **distributed in-memory data store** written in modern C++17. It integrates concurrency, fault tolerance, and analytics capabilities into one cohesive solution, showcasing advanced C++ features suitable for production-grade systems.

---

## 📚 Table of Contents
1. [Overview](#overview)
2. [Key Features](#key-features)
   - [Distribution & Consistent Hashing](#distribution--consistent-hashing)
   - [Concurrency & Low Latency](#concurrency--low-latency)
   - [Fault Tolerance & WAL](#fault-tolerance--wal)
   - [Columnar Analytics & GPU Support](#columnar-analytics--gpu-support)
   - [Networking & Replication](#networking--replication)
3. [Project Architecture](#project-architecture)
4. [Folder Structure](#folder-structure)
5. [Dependencies & Setup](#dependencies--setup)
6. [How to Build](#how-to-build)
   - [Enabling CUDA](#enabling-cuda)
7. [Running the Demo](#running-the-demo)
8. [Running the Tests](#running-the-tests)
   - [Timing Measurements](#timing-measurements)
9. [Design Details](#design-details)
   - [ThreadPool & Concurrency](#threadpool--concurrency)
   - [LockFreeRingBuffer](#lockfreeringbuffer)
   - [ConsistentHashRing](#consistenthashring)
   - [ConcurrentHashMap](#concurrenthashmap)
   - [Write-Ahead Log (WAL)](#write-ahead-log-wal)
   - [ColumnarTable](#columnartable)
   - [GPUAcceleratedAnalytics](#gpuacceleratedanalytics)
   - [DistributedNode](#distributednode)
10. [Future Improvements](#future-improvements)
11. [License](#license)

---

## 🌟 Overview

The **High-Performance Distributed In-Memory Data Store** aims to demonstrate:
- **Scalable distribution** with consistent hashing.
- **Low-latency** concurrency via a custom **ThreadPool** and **LockFreeRingBuffer**.
- **Fault tolerance** using a simple Write-Ahead Log (WAL) for crash recovery.
- **Analytics** using a columnar data structure for fast filtering.
- **Optional GPU support** for accelerating analytics.
- **Networking** to handle cross-node replication and client requests over TCP sockets.

This codebase is optimized for speed and clarity, showcasing best practices in C++17. It emphasizes multi-threaded design, lock-free data structures, and data distribution patterns commonly found in modern high-throughput systems.

---

## 🛠️ Key Features

### Distribution & Consistent Hashing
- **ConsistentHashRing**:
  - Allows seamless scaling.
  - When a node is added or removed, only a fraction of keys need to be moved.

### Concurrency & Low Latency
- **ThreadPool**:
  - A pool of worker threads manages tasks concurrently.
  - Prevents the overhead of constantly spawning threads.
- **LockFreeRingBuffer**:
  - Demonstrates a single-producer / single-consumer queue with zero locks.
  - Minimal overhead for real-time message passing.

### Fault Tolerance & WAL
- **WriteAheadLog**:
  - Before any `PUT` or `REMOVE` is applied in-memory, it is written to disk.
  - On startup, logs are **replayed** to restore the last known state.

### Columnar Analytics & GPU Support
- **ColumnarTable**:
  - Stores data in columns instead of rows, accelerating filter-based queries.
  - Offers `filterLessThan` functionality to quickly count matching rows.
- **GPUAcceleratedAnalytics**:
  - If compiled with `-DUSE_CUDA=ON`, a simple CUDA kernel performs GPU-based filtering.
  - Falls back to CPU if CUDA is unavailable.

### Networking & Replication
- **DistributedNode**:
  - Each node hosts a TCP server to handle GET/PUT/REMOVE commands.
  - Replicates data to peer nodes for redundancy.

---

## Project Architecture

1. **Main**: Runs a **demo** (unless in `UNIT_TEST` mode).
2. **Distributed Nodes**: Each runs a server on a distinct port to accept commands.
3. **ConsistentHashRing**: Decides which node is responsible for a given key.
4. **Concurrency**:
   - ThreadPool for parallel tasks.
   - LockFreeRingBuffer for a SPSC queue.
5. **Data**:
   - Stored in a **ConcurrentHashMap** on each node.
   - Backed by **WriteAheadLog** for crash recovery.
6. **Analytics**:
   - **ColumnarTable** with potential GPU acceleration.

---

## Folder Structure
```
dist_data_store/
├── include/
│   ├── concurrency.hpp
│   ├── datastore.hpp
│   └── distributed_node.hpp
├── src/
│   ├── concurrency.cpp
│   ├── datastore.cpp
│   ├── distributed_node.cpp
│   └── main.cpp
├── tests/
│   ├── test_main.cpp
│   └── test_suite.cpp
├── CMakeLists.txt
└── README.md
```

---

## Dependencies & Setup

- **C++17** compiler (e.g., `g++ 9+` or Clang 9+).
- **CMake** >= 3.10.
- **Google Test** (optional, automatically downloaded on some systems or specified via your package manager).
- **CUDA Toolkit** (optional, if `USE_CUDA` is enabled).

> **Note**: On most Linux or macOS systems, installing `cmake` and a modern `gcc` or `clang` is sufficient to get started.

---

## How to Build

1. **Clone** this repository:
   ```bash
   git clone https://github.com/maazfaisal02/High-Performance-Distributed-In-Memory-Data-Store.git
   cd dist_data_store
2. Generate build files with CMake:
   ```bash
   mkdir build && cd build
   cmake ..
3. Compile:
   ```bash
   make

If everything goes well, two main executables should appear in your build/ directory:
- dist_demo: The main demo application.
- dist_tests: The Google Test suite (only if BUILD_TESTS=ON).

## Enabling CUDA

If you have a GPU and CUDA drivers installed, you can enable CUDA-based analytics:

```bash
cmake .. -DUSE_CUDA=ON
make
```
This will compile the optional GPU kernel code in `datastore.cpp` for **GPUAcceleratedAnalytics**.

---

# Running the Demo

From the `build` directory, run:

```bash
./dist_demo
```
## What happens:
1. Two `DistributedNode` instances are created: `nodeA` (port 5001) and `nodeB` (port 5002).
2. A `ConsistentHashRing` maps each key (`IBM`, `AAPL`, `GOOG`, `TSLA`) to either `nodeA` or `nodeB`.
3. Data is **replicated** to the other node for redundancy.
4. A `ColumnarTable` is created and used to demonstrate filter-based analytics.
5. (Optional) GPU-accelerated filtering is showcased, if CUDA was enabled.
6. The demo sleeps for 2 seconds, then exits.

During execution, it prints detailed logs explaining:
- Which node is responsible for each key.
- The final results of retrieving them.
- The analytics results (including any GPU computations).

# Running the Tests

From the `build` directory:

```bash
./dist_tests
```

The test suites cover:

- `ConsistentHashRing`: distribution logic
- `ConcurrentHashMap`: concurrency correctness
- `WriteAheadLog`: replay logic
- `ColumnarTable`: filtering & row/column checks
- `GPUAcceleratedAnalytics`: fallback or CUDA-based filtering
- `DistributedNode`: GET/PUT/REMOVE operations
- `LockFreeRingBuffer` & `ThreadPool`: concurrency patterns

## Timing Measurements

A Google Test fixture records the microseconds (µs) each test takes. After each test, you’ll see an output line like:

```css
[--- Test took 123 µs ---]
```
This helps confirm **low-latency** performance for concurrency and data structures.

---

# Design Details

## ThreadPool & Concurrency
- `ThreadPool` uses a `std::list<std::function<void()>>` of tasks.
- Each worker thread waits on a condition variable until tasks are available, ensuring minimal overhead.

## LockFreeRingBuffer
- Size + 1 internal array to avoid the classic “off-by-one” problem in circular queues.
- `head_` and `tail_` are atomic pointers for single-producer/single-consumer usage.

## ConsistentHashRing
- Uses a `std::map<size_t, std::string>` to store virtual replicas (hash values).
- `getNode(key)` uses `std::hash<std::string>()(key)` to find the node.

## ConcurrentHashMap
- Wrapped in a `std::mutex` for safe access in multi-threaded contexts.
- Basic `put`, `get`, and `remove` methods.

## Write-Ahead Log (WAL)
- On each `PUT` or `REMOVE`, we append to `wal.log`.
- On node startup, the WAL is replayed to restore state before serving any requests.

## ColumnarTable
- Stores each column as a separate `std::vector<int>`.
- Filter operations (`filterLessThan`) can quickly scan only the relevant column.

## GPUAcceleratedAnalytics
- If compiled with CUDA, a simple kernel performs the filter and uses an `atomicAdd` to count matches.
- Falls back to a straightforward CPU loop if no CUDA is available.

## DistributedNode
- Runs a TCP server listening for commands:
  - `PUT key value`
  - `GET key`
  - `REMOVE key`
- Commands are parsed, then the node updates its in-memory store or returns data if available.
- If `PUT` is handled, the node also replicates the data to a peer for redundancy.

---

# Future Improvements

1. **Multi-threaded Columnar Queries**: Offload large analytics queries across multiple threads or multiple GPU streams.

2. **Node Discovery & Cluster Membership**: Automatic detection of new or removed nodes, with key rebalance.

3. **Security & Authentication**: TLS/SSL support, user authentication, and role-based access for enterprise usage.

4. **Advanced GPU Analytics**: Kernels for aggregations, joins, or streaming updates.

5. **Replication Consistency Modes**: Support eventual vs. strong consistency via user-specified replication factor.


---

# Closing Thoughts

This project highlights a **fast**, **scalable**, and **fault-tolerant** data store architecture, combining modern C++ concurrency with distributed systems design. Feel free to explore, adapt, or extend the project.

Contributions via pull requests or issues are always welcome!
