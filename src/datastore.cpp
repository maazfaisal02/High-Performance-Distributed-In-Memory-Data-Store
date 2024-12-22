#include "datastore.hpp"
#include <functional>
#include <cmath>

/******************************************************************************
 * ConsistentHashRing
 *****************************************************************************/
ConsistentHashRing::ConsistentHashRing(int numReplicas)
    : numReplicas_(numReplicas) {}

void ConsistentHashRing::addNode(const std::string& nodeName) {
    for (int i = 0; i < numReplicas_; ++i) {
        std::string replica = nodeName + "#" + std::to_string(i);
        size_t hashVal = std::hash<std::string>()(replica);
        ring_[hashVal] = nodeName;
    }
}

void ConsistentHashRing::removeNode(const std::string& nodeName) {
    for (int i = 0; i < numReplicas_; ++i) {
        std::string replica = nodeName + "#" + std::to_string(i);
        size_t hashVal = std::hash<std::string>()(replica);
        ring_.erase(hashVal);
    }
}

std::string ConsistentHashRing::getNode(const std::string& key) const {
    if (ring_.empty()) {
        return "";
    }
    size_t hashVal = std::hash<std::string>()(key);
    auto it = ring_.lower_bound(hashVal);
    if (it == ring_.end()) {
        it = ring_.begin();
    }
    return it->second;
}

/******************************************************************************
 * ConcurrentHashMap
 *****************************************************************************/
void ConcurrentHashMap::put(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lg(mtx_);
    kvStore_[key] = value;
}

bool ConcurrentHashMap::get(const std::string &key, std::string &outVal) const {
    std::lock_guard<std::mutex> lg(mtx_);
    auto it = kvStore_.find(key);
    if (it != kvStore_.end()) {
        outVal = it->second;
        return true;
    }
    return false;
}

bool ConcurrentHashMap::remove(const std::string &key) {
    std::lock_guard<std::mutex> lg(mtx_);
    return kvStore_.erase(key) > 0;
}

/******************************************************************************
 * WriteAheadLog
 *****************************************************************************/
WriteAheadLog::WriteAheadLog(const std::string &filename)
    : filename_(filename)
{
    walStream_.open(filename_, std::ios::app | std::ios::out);
    CHECK_RET(walStream_.is_open(), "Failed to open WAL file: " + filename_);
}

WriteAheadLog::~WriteAheadLog() {
    if (walStream_.is_open()) {
        walStream_.close();
    }
}

void WriteAheadLog::logPut(const std::string &key, const std::string &value) {
    std::lock_guard<std::mutex> lock(mtx_);
    walStream_ << "PUT " << key << " " << value << "\n";
    walStream_.flush();
}

void WriteAheadLog::logRemove(const std::string &key) {
    std::lock_guard<std::mutex> lock(mtx_);
    walStream_ << "REMOVE " << key << "\n";
    walStream_.flush();
}

void WriteAheadLog::replay(ConcurrentHashMap &store) {
    std::ifstream in(filename_);
    CHECK_RET(in.is_open(), "Failed to open WAL file for replay: " + filename_);
    std::string cmd;
    while (true) {
        if (!(in >> cmd)) {
            break;
        }
        if (cmd == "PUT") {
            std::string key, value;
            in >> key >> value;
            store.put(key, value);
        } else if (cmd == "REMOVE") {
            std::string key;
            in >> key;
            store.remove(key);
        }
    }
}

/******************************************************************************
 * ColumnarTable
 *****************************************************************************/
void ColumnarTable::addRow(const std::vector<int> &row) {
    if (columns_.empty()) {
        columns_.resize(row.size());
    }
    // Ensure we have enough columns to store this row
    if (row.size() > columns_.size()) {
        columns_.resize(row.size());
    }
    for (size_t i = 0; i < row.size(); ++i) {
        columns_[i].push_back(row[i]);
    }
}

size_t ColumnarTable::filterLessThan(size_t colIndex, int value) const {
    if (colIndex >= columns_.size()) {
        return 0;
    }
    const auto &col = columns_[colIndex];
    size_t count = 0;
    for (int cell : col) {
        if (cell < value) {
            ++count;
        }
    }
    return count;
}

const std::vector<int>& ColumnarTable::getColumn(size_t colIndex) const {
    return columns_.at(colIndex);
}

size_t ColumnarTable::getNumRows() const {
    // Assuming all columns are same length
    if (columns_.empty()) return 0;
    return columns_[0].size();
}

/******************************************************************************
 * GPUAcceleratedAnalytics
 *****************************************************************************/
size_t GPUAcceleratedAnalytics::filterLessThanGPU(const std::vector<int> &col, int value) {
#ifdef USE_CUDA
    // Example kernel
    auto gpuFilterKernel = [] __global__ (const int* d_col, size_t size, int v, int* d_result){
        int idx = blockIdx.x * blockDim.x + threadIdx.x;
        if (idx < size) {
            if (d_col[idx] < v) {
                atomicAdd(d_result, 1);
            }
        }
    };

    int *d_col = nullptr;
    int *d_result = nullptr;
    int h_result = 0;
    const size_t size = col.size();

    cudaMalloc(&d_col, size * sizeof(int));
    cudaMalloc(&d_result, sizeof(int));
    cudaMemset(d_result, 0, sizeof(int));

    cudaMemcpy(d_col, col.data(), size * sizeof(int), cudaMemcpyHostToDevice);

    int blockSize = 256;
    int gridSize = (size + blockSize - 1) / blockSize;

    gpuFilterKernel<<<gridSize, blockSize>>>(d_col, size, value, d_result);
    cudaMemcpy(&h_result, d_result, sizeof(int), cudaMemcpyDeviceToHost);

    cudaFree(d_col);
    cudaFree(d_result);

    return static_cast<size_t>(h_result);
#else
    // Fallback if no CUDA
    size_t count = 0;
    for (auto &v : col) {
        if (v < value) {
            ++count;
        }
    }
    return count;
#endif
}
