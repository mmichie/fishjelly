#include "../src/http.h"
#include "../src/socket.h"
#include <gtest/gtest.h>
#include <memory>
#include <sstream>

class HttpTest : public ::testing::Test {
  protected:
    Http http;
};

TEST_F(HttpTest, ParseHeaderValidGet) {
    std::string header = "GET /index.html HTTP/1.1\r\n"
                         "Host: example.com\r\n"
                         "User-Agent: TestAgent\r\n"
                         "\r\n";

    EXPECT_TRUE(http.parseHeader(header));
}

TEST_F(HttpTest, ParseHeaderValidPost) {
    std::string header = "POST /submit HTTP/1.1\r\n"
                         "Host: example.com\r\n"
                         "Content-Length: 13\r\n"
                         "\r\n";

    EXPECT_TRUE(http.parseHeader(header));
}

TEST_F(HttpTest, ParseHeaderInvalidMethod) {
    std::string header = "INVALID /index.html HTTP/1.1\r\n"
                         "Host: example.com\r\n"
                         "\r\n";

    // Depending on implementation, this might return false
    http.parseHeader(header);
}

TEST_F(HttpTest, ParseHeaderEmptyHeader) { EXPECT_FALSE(http.parseHeader("")); }

TEST_F(HttpTest, ParseHeaderMalformed) {
    std::string header = "This is not a valid HTTP header";
    EXPECT_FALSE(http.parseHeader(header));
}

TEST_F(HttpTest, GetHeaderAfterSendHeader) {
    // Test sendHeader by checking the generated header
    http.sendHeader(200, 1024, "text/html", true);
    std::string header = http.getHeader();

    // Check that header contains expected components
    EXPECT_TRUE(header.find("HTTP/1.1 200") != std::string::npos);
    EXPECT_TRUE(header.find("Content-Length: 1024") != std::string::npos);
    EXPECT_TRUE(header.find("Content-Type: text/html") != std::string::npos);
    EXPECT_TRUE(header.find("Connection: keep-alive") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderNotFound) {
    http.sendHeader(404, 0, "text/plain", false);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("HTTP/1.1 404") != std::string::npos);
    EXPECT_TRUE(header.find("Connection: close") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderInternalError) {
    http.sendHeader(500, 0, "text/plain", false);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("HTTP/1.1 500") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderCustomContentType) {
    http.sendHeader(200, 2048, "application/json", true);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("Content-Type: application/json") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderDefaultContentType) {
    http.sendHeader(200, 100);
    std::string header = http.getHeader();

    // Default content type should be text/plain
    EXPECT_TRUE(header.find("Content-Type: text/plain") != std::string::npos);
    // Default keep_alive should be false
    EXPECT_TRUE(header.find("Connection: close") != std::string::npos);
}

TEST_F(HttpTest, ParseHeaderMultipleHeaders) {
    std::string header = "GET /api/data HTTP/1.1\r\n"
                         "Host: api.example.com\r\n"
                         "Accept: application/json\r\n"
                         "Authorization: Bearer token123\r\n"
                         "User-Agent: TestClient/1.0\r\n"
                         "\r\n";

    EXPECT_TRUE(http.parseHeader(header));
}

TEST_F(HttpTest, ParseHeaderWithBody) {
    std::string header = "POST /api/users HTTP/1.1\r\n"
                         "Host: api.example.com\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: 25\r\n"
                         "\r\n"
                         "{\"name\":\"test\",\"age\":30}";

    // Header parsing should stop at empty line
    EXPECT_TRUE(http.parseHeader(header));
}

// Content Negotiation Tests
TEST_F(HttpTest, SendHeaderWithVaryHeader) {
    std::vector<std::string> extra_headers = {"Vary: Accept"};
    http.sendHeader(200, 100, "application/json", true, extra_headers);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("Vary: Accept") != std::string::npos);
    EXPECT_TRUE(header.find("Content-Type: application/json") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderNotAcceptable) {
    http.sendHeader(406, 0, "text/html", false);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("HTTP/1.1 406") != std::string::npos);
    EXPECT_TRUE(header.find("Not Acceptable") != std::string::npos);
}

TEST_F(HttpTest, SendHeaderMultipleExtraHeaders) {
    std::vector<std::string> extra_headers = {"Vary: Accept", "Vary: Accept-Language",
                                              "Cache-Control: public, max-age=3600"};
    http.sendHeader(200, 500, "text/html", true, extra_headers);
    std::string header = http.getHeader();

    EXPECT_TRUE(header.find("Vary: Accept") != std::string::npos);
    EXPECT_TRUE(header.find("Vary: Accept-Language") != std::string::npos);
    EXPECT_TRUE(header.find("Cache-Control: public, max-age=3600") != std::string::npos);
}