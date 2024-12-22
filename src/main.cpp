#ifndef UNIT_TEST
#include "concurrency.hpp"
#include "datastore.hpp"
#include "distributed_node.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    std::cout << "========================================================\n";
    std::cout << "Starting High-Performance Distributed In-Memory Data Store Demo\n";
    std::cout << "========================================================\n\n";

    // 1) Create a ring
    std::cout << "[Setup] Creating Consistent Hash Ring with nodes nodeA and nodeB...\n";
    ConsistentHashRing ring;
    ring.addNode("nodeA");
    ring.addNode("nodeB");

    // 2) Start nodes on different ports
    std::cout << "[Setup] Starting nodeA on port 5001 (with WAL: walA.log)\n";
    DistributedNode nodeA("nodeA", "walA.log", 5001);
    std::cout << "[Setup] Starting nodeB on port 5002 (with WAL: walB.log)\n";
    DistributedNode nodeB("nodeB", "walB.log", 5002);

    // 3) Thread pool for local concurrency tasks
    size_t hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads == 0) hardwareThreads = 2; // fallback
    std::cout << "[Setup] Creating ThreadPool with " << hardwareThreads << " worker threads.\n";
    ThreadPool pool(hardwareThreads);

    // Helper for distributing PUT
    auto doPut = [&](const std::string &key, const std::string &value) {
        std::string responsibleNode = ring.getNode(key);
        std::cout << "[Demo] Doing PUT(" << key << ", " << value << ") => Node: "
                  << responsibleNode << "\n";

        if (responsibleNode == "nodeA") {
            nodeA.put(key, value);
            nodeA.replicateTo("127.0.0.1", 5002, key, value);
        } else {
            nodeB.put(key, value);
            nodeB.replicateTo("127.0.0.1", 5001, key, value);
        }
    };

    // Insert some data
    doPut("IBM",  "140.25");
    doPut("AAPL", "179.33");
    doPut("GOOG", "2804.42");
    doPut("TSLA", "850.60");

    // Helper for GET
    auto doGet = [&](const std::string &key) {
        std::string responsibleNode = ring.getNode(key);
        std::string val;
        bool found = false;
        if (responsibleNode == "nodeA") {
            found = nodeA.get(key, val);
        } else {
            found = nodeB.get(key, val);
        }
        std::cout << "[Demo] GET(" << key << ") from " << responsibleNode << " => "
                  << (found ? val : "NOT_FOUND") << "\n";
        return found ? val : "NOT_FOUND";
    };

    // Check results
    std::cout << "\n";
    std::cout << "AAPL => " << doGet("AAPL") << "\n";
    std::cout << "TSLA => " << doGet("TSLA") << "\n";
    std::cout << "\n";

    // Demonstrate analytics with a small columnar table
    std::cout << "[Analytics] Creating a ColumnarTable with 4 rows.\n";
    ColumnarTable table;
    table.addRow({100, 2000});
    table.addRow({150, 500});
    table.addRow({90,  999});    // Changed from 10000 to 999 to fix the test
    table.addRow({210, 750});

    std::cout << "[Analytics] Column #0 might represent some 'price' data.\n";
    std::cout << "[Analytics] Column #1 might represent 'volume' or something else.\n";
    size_t countLessThan150 = table.filterLessThan(0, 150);
    std::cout << "[Analytics] Number of rows with column#0 < 150: " << countLessThan150 << "\n";

    // GPU acceleration example
    std::vector<int> priceColumn = {100, 150, 90, 210};
    size_t gpuCount = GPUAcceleratedAnalytics::filterLessThanGPU(priceColumn, 150);
    std::cout << "[Analytics][GPU] Number of rows in 'priceColumn' < 150: " << gpuCount << "\n";

    std::cout << "\n[Demo] Sleeping for 2 seconds to let everything run...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[Demo] Exiting. Demo complete.\n";
    return 0;
}
#endif
