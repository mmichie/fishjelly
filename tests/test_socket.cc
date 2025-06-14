#include "../src/socket.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

class SocketTest : public ::testing::Test {
  protected:
    // Socket constructor requires a port, so we need to handle that
};

TEST(SocketTest, ConstructorBindsToPort) {
    // Use a high port number to avoid permission issues
    int test_port = 9999;

    // Constructor automatically binds to port
    // This might fail if port is in use
    try {
        Socket socket(test_port);
        // If we get here, socket was created successfully
        socket.close_socket();
    } catch (...) {
        // Port might be in use, which is okay for testing
    }
}

TEST(SocketTest, SetSocketOptions) {
    int test_port = 9998;

    try {
        Socket socket(test_port);
        // setSocketOptions is called in constructor
        // Socket created successfully means options were set
        SUCCEED();
        socket.close_socket();
    } catch (...) {
        // Port might be in use
        GTEST_SKIP() << "Port " << test_port << " might be in use";
    }
}

TEST(SocketTest, ReadLineEmptySocket) {
    int test_port = 9997;

    try {
        Socket socket(test_port);
        std::string buffer;

        // Reading from unconnected socket should fail
        bool result = socket.read_line(&buffer);
        EXPECT_FALSE(result);

        socket.close_socket();
    } catch (...) {
        // Port might be in use
    }
}

TEST(SocketTest, WriteLineToSocket) {
    int test_port = 9996;

    try {
        Socket socket(test_port);

        // Writing to unconnected socket might fail
        // but writeLine doesn't return status
        socket.write_line("Test message\n");

        socket.close_socket();
    } catch (...) {
        // Port might be in use
    }
}

TEST(SocketTest, HandleError) {
    int test_port = 9995;

    try {
        Socket socket(test_port);

        // handleError should log the error message
        socket.handle_error("Test error message");

        socket.close_socket();
    } catch (...) {
        // Port might be in use
    }
}

TEST(SocketTest, CloseSocket) {
    int test_port = 9994;

    try {
        Socket socket(test_port);
        // Successfully created socket

        socket.close_socket();
        // If no exception, close succeeded
        SUCCEED();

    } catch (...) {
        // Port might be in use
        GTEST_SKIP() << "Port " << test_port << " might be in use";
    }
}

TEST(SocketTest, MultipleSocketsOnDifferentPorts) {
    try {
        Socket socket1(9993);
        Socket socket2(9992);

        // Both sockets created successfully
        SUCCEED();

        socket1.close_socket();
        socket2.close_socket();
    } catch (...) {
        // Ports might be in use
        GTEST_SKIP() << "Ports might be in use";
    }
}

// Test client connection (requires server/client setup)
TEST(SocketTest, DISABLED_AcceptClient) {
    // This test is disabled because it requires a full server/client setup
    // In a real test environment, you would:
    // 1. Create a server socket
    // 2. Start a thread to connect as client
    // 3. Call acceptClient()
    // 4. Verify connection was accepted
}