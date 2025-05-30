#include <gtest/gtest.h>
#include "../src/filter.h"

class FilterTest : public ::testing::Test {
protected:
    Filter filter;
};

TEST_F(FilterTest, AddFooterToEmptyString) {
    std::string result = filter.addFooter("");
    // Should add footer even to empty string
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.find("</") != std::string::npos || 
                result.find("footer") != std::string::npos ||
                result.length() > 0);
}

TEST_F(FilterTest, AddFooterToHtmlContent) {
    std::string html = "<html><body><h1>Hello World</h1></body></html>";
    std::string result = filter.addFooter(html);
    
    // Result should contain original content
    EXPECT_TRUE(result.find("Hello World") != std::string::npos);
    
    // Result should be longer than original (footer added)
    EXPECT_GT(result.length(), html.length());
}

TEST_F(FilterTest, AddFooterToPlainText) {
    std::string text = "This is plain text content";
    std::string result = filter.addFooter(text);
    
    // Result should contain original content
    EXPECT_TRUE(result.find("This is plain text content") != std::string::npos);
    
    // Result should have footer appended
    EXPECT_GT(result.length(), text.length());
}

TEST_F(FilterTest, AddFooterPreservesNewlines) {
    std::string text = "Line 1\nLine 2\nLine 3";
    std::string result = filter.addFooter(text);
    
    // Original newlines should be preserved
    EXPECT_TRUE(result.find("Line 1\nLine 2\nLine 3") != std::string::npos);
}

TEST_F(FilterTest, AddFooterToLargeContent) {
    // Create a large string
    std::string large_content;
    for (int i = 0; i < 1000; ++i) {
        large_content += "This is line " + std::to_string(i) + "\n";
    }
    
    std::string result = filter.addFooter(large_content);
    
    // Should handle large content
    EXPECT_GT(result.length(), large_content.length());
    
    // Should contain beginning of original content
    EXPECT_TRUE(result.find("This is line 0") != std::string::npos);
}

TEST_F(FilterTest, AddFooterMultipleCalls) {
    std::string content1 = "First content";
    std::string content2 = "Second content";
    
    std::string result1 = filter.addFooter(content1);
    std::string result2 = filter.addFooter(content2);
    
    // Each call should produce different results
    EXPECT_NE(result1, result2);
    
    // Each should contain its original content
    EXPECT_TRUE(result1.find("First content") != std::string::npos);
    EXPECT_TRUE(result2.find("Second content") != std::string::npos);
}

TEST_F(FilterTest, AddFooterSpecialCharacters) {
    std::string content = "Content with <special> & \"characters\" 'here'";
    std::string result = filter.addFooter(content);
    
    // Special characters should be preserved
    EXPECT_TRUE(result.find("<special>") != std::string::npos);
    EXPECT_TRUE(result.find("&") != std::string::npos);
    EXPECT_TRUE(result.find("\"characters\"") != std::string::npos);
}

TEST_F(FilterTest, AddFooterUnicodeContent) {
    std::string content = "Unicode: 你好世界 🌍 Ñoño";
    std::string result = filter.addFooter(content);
    
    // Unicode should be preserved
    EXPECT_TRUE(result.find("你好世界") != std::string::npos);
    EXPECT_TRUE(result.find("🌍") != std::string::npos);
    EXPECT_TRUE(result.find("Ñoño") != std::string::npos);
}