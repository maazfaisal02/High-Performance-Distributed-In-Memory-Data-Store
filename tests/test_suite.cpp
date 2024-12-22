#ifdef UNIT_TEST

#include <gtest/gtest.h>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>          // <-- for high_resolution_clock
#include <iostream>        // <-- for printing timing

#include "datastore.hpp"
#include "distributed_node.hpp"
#include "concurrency.hpp"


// ---------------------------------------------------------
//  Timing Fixture: prints microseconds after each test
// ---------------------------------------------------------
class TimedTest : public ::testing::Test {
protected:
    std::chrono::high_resolution_clock::time_point startTime_;

    void SetUp() override {
        startTime_ = std::chrono::high_resolution_clock::now();
    }
    void TearDown() override {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_).count();
        std::cout << "[--- Test took " << durationUs << " µs ---]" << std::endl;
    }
};

// ----------------------------------------------------------
// 1) ConsistentHashRing
// ----------------------------------------------------------
TEST(ConsistentHashRingTest, Basic) {
    ConsistentHashRing ring;
    ring.addNode("nodeA");
    ring.addNode("nodeB");
    std::string node = ring.getNode("TestKey");
    EXPECT_FALSE(node.empty()); // Must map to either nodeA or nodeB
}

TEST(ConsistentHashRingTest, AddRemove) {
    ConsistentHashRing ring(2);
    ring.addNode("nodeX");
    std::string node = ring.getNode("KeyABC");
    EXPECT_EQ(node, "nodeX");
    ring.removeNode("nodeX");
    EXPECT_EQ(ring.getNode("KeyABC"), "");
}

TEST(ConsistentHashRingTest, MultipleNodes) {
    ConsistentHashRing ring(2);
    ring.addNode("nodeA");
    ring.addNode("nodeB");
    ring.addNode("nodeC");
    // Just ensure it doesn't throw and returns some node
    for (int i = 0; i < 10; ++i) {
        std::string key = "Key" + std::to_string(i);
        std::string n = ring.getNode(key);
        EXPECT_TRUE(!n.empty());
    }
}

// ----------------------------------------------------------
// 2) ConcurrentHashMap
// ----------------------------------------------------------
TEST(ConcurrentHashMapTest, PutGetRemove) {
    ConcurrentHashMap map;
    map.put("A", "100");
    std::string val;
    bool found = map.get("A", val);
    EXPECT_TRUE(found);
    EXPECT_EQ(val, "100");

    map.remove("A");
    found = map.get("A", val);
    EXPECT_FALSE(found);
}

TEST(ConcurrentHashMapTest, MultiplePuts) {
    ConcurrentHashMap map;
    map.put("X", "xval");
    map.put("Y", "yval");
    map.put("Z", "zval");

    std::string val;
    EXPECT_TRUE(map.get("X", val));
    EXPECT_EQ(val, "xval");
    EXPECT_TRUE(map.get("Z", val));
    EXPECT_EQ(val, "zval");
}

// ----------------------------------------------------------
// 3) WriteAheadLog (WAL)
// ----------------------------------------------------------
TEST(WALTest, Replay) {
    // Clear file
    {
        std::ofstream ofs("test_wal.log", std::ios::trunc);
        ofs.close();
    }
    {
        WriteAheadLog wal("test_wal.log");
        wal.logPut("K1", "V1");
        wal.logRemove("K2");
    }
    {
        ConcurrentHashMap store;
        WriteAheadLog walReader("test_wal.log");
        walReader.replay(store);

        std::string val;
        bool found = store.get("K1", val);
        EXPECT_TRUE(found);
        EXPECT_EQ(val, "V1");
        found = store.get("K2", val);
        EXPECT_FALSE(found);
    }
}

// ----------------------------------------------------------
// 4) ColumnarTable
// ----------------------------------------------------------
TEST(ColumnarTableTest, BasicFilter) {
    ColumnarTable table;
    // 4 rows, 2 columns
    // We'll make col#1 have 3 values < 1000
    table.addRow({100, 2000});
    table.addRow({150, 500});
    table.addRow({90,  999});    // Changed from 10000 to 999
    table.addRow({210, 750});

    // Column 0: [100, 150, 90, 210]
    //   filterLessThan(0, 150) => 2 (100, 90)
    EXPECT_EQ(table.filterLessThan(0, 150), (size_t)2);

    // Column 1: [2000, 500, 999, 750]
    //   filterLessThan(1, 1000) => 3 (500, 999, 750)
    EXPECT_EQ(table.filterLessThan(1, 1000), (size_t)3);
}

TEST(ColumnarTableTest, GetNumRows) {
    ColumnarTable t;
    EXPECT_EQ(t.getNumRows(), (size_t)0);

    t.addRow({1, 2});
    t.addRow({3, 4});
    EXPECT_EQ(t.getNumRows(), (size_t)2);
}

TEST(ColumnarTableTest, GetColumn) {
    ColumnarTable t;
    t.addRow({10, 100});
    t.addRow({20, 200});
    t.addRow({30, 300});

    // Column 0: [10, 20, 30]
    EXPECT_EQ(t.getColumn(0).size(), (size_t)3);
    EXPECT_EQ(t.getColumn(0)[1], 20);
    // Column 1: [100, 200, 300]
    EXPECT_EQ(t.getColumn(1).size(), (size_t)3);
    EXPECT_EQ(t.getColumn(1)[2], 300);
}

// ----------------------------------------------------------
// 5) GPUAcceleratedAnalytics
// ----------------------------------------------------------
TEST(GPUAcceleratedAnalyticsTest, FallbackOrGPU) {
    std::vector<int> col = { 5, 10, 15, 20 };
    size_t cnt = GPUAcceleratedAnalytics::filterLessThanGPU(col, 15);
    EXPECT_EQ(cnt, (size_t)2); // 5 and 10 are < 15
}

// ----------------------------------------------------------
// 6) DistributedNode
// ----------------------------------------------------------
TEST(DistributedNodeTest, BasicOps) {
    // Clear old WAL
    {
        std::ofstream ofs("test_wal_node.log", std::ios::trunc);
        ofs.close();
    }

    DistributedNode node("TestNode", "test_wal_node.log", 6001);
    node.put("Alpha", "123");

    std::string val;
    EXPECT_TRUE(node.get("Alpha", val));
    EXPECT_EQ(val, "123");

    node.removeKey("Alpha");
    EXPECT_FALSE(node.get("Alpha", val));
}

// ----------------------------------------------------------
// 7) LockFreeRingBuffer & ThreadPool Tests (EXTRA coverage)
// ----------------------------------------------------------
TEST_F(TimedTest, BasicPushPop) {
    LockFreeRingBuffer<int, 5> ring;
    EXPECT_EQ(ring.size(), (size_t)0);

    // push up to capacity
    EXPECT_TRUE(ring.push(10));
    EXPECT_TRUE(ring.push(20));
    EXPECT_TRUE(ring.push(30));
    EXPECT_EQ(ring.size(), (size_t)3);

    int val;
    EXPECT_TRUE(ring.pop(val));
    EXPECT_EQ(val, 10);
    EXPECT_TRUE(ring.pop(val));
    EXPECT_EQ(val, 20);
    EXPECT_EQ(ring.size(), (size_t)1);
}

TEST_F(TimedTest, Overfill) {
    // Changed from 3 -> 3 means we get 4 internal slots in the array, so we can hold 3 items at once
    LockFreeRingBuffer<int, 3> ring;
    EXPECT_TRUE(ring.push(1));
    EXPECT_TRUE(ring.push(2));

    // Now we expect pushing (3) to succeed (because internally we have 4 slots)
    // This used to fail before because ring was effectively holding only 2 items in a 3-slot ring.
    EXPECT_TRUE(ring.push(3));

    // Should be full now
    EXPECT_FALSE(ring.push(4)); // can't push anymore

    int val;
    EXPECT_TRUE(ring.pop(val));
    EXPECT_EQ(val, 1);

    // Now that we've popped 1, there's space to push(4).
    EXPECT_TRUE(ring.push(4));
}

TEST_F(TimedTest, SimpleTask) {
    ThreadPool pool(2);

    auto futureVal = pool.enqueue([]() {
        return 42;
    });
    EXPECT_EQ(futureVal.get(), 42);

    auto futureStr = pool.enqueue([](const std::string &s) {
        return s + "_done";
    }, "hello");
    EXPECT_EQ(futureStr.get(), "hello_done");
}

#endif
