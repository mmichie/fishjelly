#include <gtest/gtest.h>
#include "../src/token.h"

class TokenTest : public ::testing::Test {
protected:
    Token token;
};

TEST_F(TokenTest, TokenizeBasicString) {
    std::vector<std::string> tokens;
    token.tokenize("hello world test", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(TokenTest, TokenizeEmptyString) {
    std::vector<std::string> tokens;
    token.tokenize("", tokens, " ");
    
    EXPECT_TRUE(tokens.empty());
}

TEST_F(TokenTest, TokenizeNoDelimiter) {
    std::vector<std::string> tokens;
    token.tokenize("helloworld", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0], "helloworld");
}

TEST_F(TokenTest, TokenizeMultipleDelimiters) {
    std::vector<std::string> tokens;
    token.tokenize("hello,,world", tokens, ",");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "");
    EXPECT_EQ(tokens[2], "world");
}

TEST_F(TokenTest, TokenizeTrailingDelimiter) {
    std::vector<std::string> tokens;
    token.tokenize("hello world ", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "");
}

TEST_F(TokenTest, TokenizeLeadingDelimiter) {
    std::vector<std::string> tokens;
    token.tokenize(" hello world", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "");
    EXPECT_EQ(tokens[1], "hello");
    EXPECT_EQ(tokens[2], "world");
}

TEST_F(TokenTest, TokenizeMultiCharDelimiter) {
    std::vector<std::string> tokens;
    token.tokenize("hello::world::test", tokens, "::");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "hello");
    EXPECT_EQ(tokens[1], "world");
    EXPECT_EQ(tokens[2], "test");
}

TEST_F(TokenTest, TokenizeWithNewline) {
    std::vector<std::string> tokens;
    token.tokenize("line1\nline2\nline3", tokens, "\n");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "line1");
    EXPECT_EQ(tokens[1], "line2");
    EXPECT_EQ(tokens[2], "line3");
}

TEST_F(TokenTest, TokenizeWithTab) {
    std::vector<std::string> tokens;
    token.tokenize("col1\tcol2\tcol3", tokens, "\t");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "col1");
    EXPECT_EQ(tokens[1], "col2");
    EXPECT_EQ(tokens[2], "col3");
}

TEST_F(TokenTest, TokenizeLongString) {
    std::vector<std::string> tokens;
    std::string longStr = "This is a very long string with many words to test the tokenizer performance and correctness";
    token.tokenize(longStr, tokens, " ");
    
    ASSERT_EQ(tokens.size(), 16u);
    EXPECT_EQ(tokens[0], "This");
    EXPECT_EQ(tokens[15], "correctness");
}

TEST_F(TokenTest, TokenizeOnlyDelimiters) {
    std::vector<std::string> tokens;
    token.tokenize("   ", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 4u);
    EXPECT_TRUE(tokens[0].empty());
    EXPECT_TRUE(tokens[1].empty());
    EXPECT_TRUE(tokens[2].empty());
    EXPECT_TRUE(tokens[3].empty());
}

TEST_F(TokenTest, TokenizeHttpRequest) {
    std::vector<std::string> tokens;
    token.tokenize("GET /index.html HTTP/1.1", tokens, " ");
    
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "GET");
    EXPECT_EQ(tokens[1], "/index.html");
    EXPECT_EQ(tokens[2], "HTTP/1.1");
}