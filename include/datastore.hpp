#ifndef DATASTORE_HPP
#define DATASTORE_HPP

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>

// If you want GPU, compile with -DUSE_CUDA
#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

// Use the same CHECK_RET macro from concurrency.hpp if needed
#ifndef CHECK_RET
#define CHECK_RET(cond, msg) \
    if (!(cond)) {           \
        std::cerr << (msg) << std::endl; \
        std::exit(EXIT_FAILURE);         \
    }
#endif

/**
 * ConsistentHashRing
 */
class ConsistentHashRing {
public:
    explicit ConsistentHashRing(int numReplicas = 100);
    void addNode(const std::string& nodeName);
    void removeNode(const std::string& nodeName);
    std::string getNode(const std::string& key) const;
private:
    std::map<size_t, std::string> ring_;
    int numReplicas_;
};

/**
 * ConcurrentHashMap
 */
class ConcurrentHashMap {
public:
    void put(const std::string &key, const std::string &value);
    bool get(const std::string &key, std::string &outVal) const;
    bool remove(const std::string &key);
private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::string> kvStore_;
};

/**
 * Write-Ahead Log (WAL)
 */
class WriteAheadLog {
public:
    explicit WriteAheadLog(const std::string &filename);
    ~WriteAheadLog();

    void logPut(const std::string &key, const std::string &value);
    void logRemove(const std::string &key);
    void replay(ConcurrentHashMap &store);

private:
    std::mutex mtx_;
    std::ofstream walStream_;
    std::string filename_;
};

/**
 * ColumnarTable for analytics
 */
class ColumnarTable {
public:
    void addRow(const std::vector<int> &row);
    size_t filterLessThan(size_t colIndex, int value) const;
    const std::vector<int>& getColumn(size_t colIndex) const;

    // Optional: get total number of rows
    size_t getNumRows() const;

private:
    std::vector<std::vector<int>> columns_;
};

/**
 * GPUAcceleratedAnalytics (stub)
 */
class GPUAcceleratedAnalytics {
public:
    static size_t filterLessThanGPU(const std::vector<int> &col, int value);
};

#endif // DATASTORE_HPP
