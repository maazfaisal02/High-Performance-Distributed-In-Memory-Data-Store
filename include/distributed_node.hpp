#ifndef DISTRIBUTED_NODE_HPP
#define DISTRIBUTED_NODE_HPP

#include <atomic>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iostream>

#include "concurrency.hpp"
#include "datastore.hpp"

class DistributedNode {
public:
    DistributedNode(const std::string &nodeName,
                    const std::string &walFile,
                    int port);
    ~DistributedNode();

    void put(const std::string &key, const std::string &value);
    bool get(const std::string &key, std::string &outVal);
    void removeKey(const std::string &key);
    std::string getName() const;

    // Replicate a key/value to another node
    void replicateTo(const std::string &targetHost, int targetPort,
                     const std::string &key, const std::string &value);

private:
    void runServer();
    void handleClient(int clientSock);

    std::string nodeName_;
    ConcurrentHashMap dataStore_;
    WriteAheadLog wal_;
    int port_;
    std::atomic<bool> stop_;
    std::thread serverThread_;

    // Helper to forcibly unblock accept()
    void forceDisconnect();
};

#endif // DISTRIBUTED_NODE_HPP
