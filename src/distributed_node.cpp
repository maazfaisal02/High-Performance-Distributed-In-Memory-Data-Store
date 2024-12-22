#include "distributed_node.hpp"
#include <iostream>

DistributedNode::DistributedNode(const std::string &nodeName,
                                 const std::string &walFile,
                                 int port)
    : nodeName_(nodeName), wal_(walFile), port_(port), stop_(false)
{
    // Replay WAL to restore data
    wal_.replay(dataStore_);

    // Start the server thread
    serverThread_ = std::thread(&DistributedNode::runServer, this);
}

DistributedNode::~DistributedNode() {
    stop_ = true;
    // Force accept() to unblock by doing a loopback connect
    forceDisconnect();
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void DistributedNode::put(const std::string &key, const std::string &value) {
    dataStore_.put(key, value);
    wal_.logPut(key, value);
}

bool DistributedNode::get(const std::string &key, std::string &outVal) {
    return dataStore_.get(key, outVal);
}

void DistributedNode::removeKey(const std::string &key) {
    dataStore_.remove(key);
    wal_.logRemove(key);
}

std::string DistributedNode::getName() const {
    return nodeName_;
}

void DistributedNode::replicateTo(const std::string &targetHost,
                                  int targetPort,
                                  const std::string &key,
                                  const std::string &value)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_RET(sock >= 0, "Failed to create socket in replicateTo");

    sockaddr_in servAddr;
    std::memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(targetPort);
    servAddr.sin_addr.s_addr = inet_addr(targetHost.c_str());

    int c = connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr));
    if (c < 0) {
        // Could not connect; just close & return
        close(sock);
        return;
    }

    std::string msg = "PUT " + key + " " + value + "\n";
    ::send(sock, msg.data(), msg.size(), 0);
    close(sock);
}

void DistributedNode::runServer() {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_RET(serverSock >= 0, "Failed to create server socket");

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    CHECK_RET(bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) >= 0,
              "Failed to bind server socket");
    CHECK_RET(listen(serverSock, 10) >= 0, "Failed to listen on server socket");

    while (!stop_) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSock = accept(serverSock, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSock < 0) {
            if (stop_) {
                break;
            }
            continue;
        }
        handleClient(clientSock);
    }
    close(serverSock);
}

void DistributedNode::handleClient(int clientSock) {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    int bytesRead = static_cast<int>(recv(clientSock, buffer, sizeof(buffer) - 1, 0));
    if (bytesRead > 0) {
        std::string request(buffer, bytesRead);
        std::istringstream iss(request);
        std::string cmd;
        iss >> cmd;
        if (cmd == "PUT") {
            std::string key, value;
            iss >> key >> value;
            put(key, value);
        } else if (cmd == "REMOVE") {
            std::string key;
            iss >> key;
            removeKey(key);
        } else if (cmd == "GET") {
            std::string key;
            iss >> key;
            std::string val;
            if (get(key, val)) {
                std::string resp = "VALUE " + val + "\n";
                ::send(clientSock, resp.data(), resp.size(), 0);
            } else {
                std::string resp = "NOT_FOUND\n";
                ::send(clientSock, resp.data(), resp.size(), 0);
            }
        }
    }
    close(clientSock);
}

/**
 * Helper to forcibly unblock accept() by connecting to this node.
 */
void DistributedNode::forceDisconnect() {
    int dummySock = socket(AF_INET, SOCK_STREAM, 0);
    if (dummySock < 0) {
        return;
    }

    sockaddr_in dummyAddr;
    std::memset(&dummyAddr, 0, sizeof(dummyAddr));
    dummyAddr.sin_family = AF_INET;
    dummyAddr.sin_port = htons(port_);
    dummyAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(dummySock, (struct sockaddr*)&dummyAddr, sizeof(dummyAddr));
    close(dummySock);
}
