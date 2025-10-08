#include "../src/content_negotiator.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class ContentNegotiatorTest : public ::testing::Test {
  protected:
    ContentNegotiator negotiator;
    std::string test_dir;

    void SetUp() override {
        // Create a temporary test directory with variant files
        test_dir = "test_content_negotiation";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test directory
        std::filesystem::remove_all(test_dir);
    }

    // Helper to create test files
    void createTestFile(const std::string& path, const std::string& content = "test content") {
        std::ofstream file(path);
        file << content;
        file.close();
    }
};

// Test MediaType construction and specificity
TEST_F(ContentNegotiatorTest, MediaTypeSpecificity) {
    MediaType exact("text/html", 1.0);
    EXPECT_EQ(exact.specificity, 3);

    MediaType wildcard_subtype("text/*", 1.0);
    EXPECT_EQ(wildcard_subtype.specificity, 2);

    MediaType wildcard_all("*/*", 1.0);
    EXPECT_EQ(wildcard_all.specificity, 1);
}

// Test MediaType matching
TEST_F(ContentNegotiatorTest, MediaTypeMatching) {
    MediaType exact("text/html", 1.0);
    EXPECT_TRUE(exact.matches("text/html"));
    EXPECT_FALSE(exact.matches("text/plain"));
    EXPECT_FALSE(exact.matches("application/json"));

    MediaType wildcard_subtype("text/*", 1.0);
    EXPECT_TRUE(wildcard_subtype.matches("text/html"));
    EXPECT_TRUE(wildcard_subtype.matches("text/plain"));
    EXPECT_FALSE(wildcard_subtype.matches("application/json"));

    MediaType wildcard_all("*/*", 1.0);
    EXPECT_TRUE(wildcard_all.matches("text/html"));
    EXPECT_TRUE(wildcard_all.matches("application/json"));
    EXPECT_TRUE(wildcard_all.matches("image/png"));
}

// Test Accept header parsing - basic
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderBasic) {
    auto types = negotiator.parseAcceptHeader("text/html, application/json");
    EXPECT_EQ(types.size(), 2u);
    EXPECT_EQ(types[0].type, "text/html");
    EXPECT_DOUBLE_EQ(types[0].quality, 1.0);
    EXPECT_EQ(types[1].type, "application/json");
    EXPECT_DOUBLE_EQ(types[1].quality, 1.0);
}

// Test Accept header parsing with quality values
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderWithQuality) {
    auto types = negotiator.parseAcceptHeader("text/html, application/json;q=0.9, */*;q=0.1");
    EXPECT_EQ(types.size(), 3u);

    // Should be sorted by quality (descending)
    EXPECT_EQ(types[0].type, "text/html");
    EXPECT_DOUBLE_EQ(types[0].quality, 1.0);

    EXPECT_EQ(types[1].type, "application/json");
    EXPECT_DOUBLE_EQ(types[1].quality, 0.9);

    EXPECT_EQ(types[2].type, "*/*");
    EXPECT_DOUBLE_EQ(types[2].quality, 0.1);
}

// Test Accept header parsing with whitespace
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderWithWhitespace) {
    auto types = negotiator.parseAcceptHeader("  text/html  ,  application/json ; q=0.8  ");
    EXPECT_EQ(types.size(), 2u);
    EXPECT_EQ(types[0].type, "text/html");
    EXPECT_EQ(types[1].type, "application/json");
    EXPECT_DOUBLE_EQ(types[1].quality, 0.8);
}

// Test Accept header parsing - empty
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderEmpty) {
    auto types = negotiator.parseAcceptHeader("");
    EXPECT_EQ(types.size(), 1u);
    EXPECT_EQ(types[0].type, "*/*");
    EXPECT_DOUBLE_EQ(types[0].quality, 1.0);
}

// Test Accept header parsing with specificity ordering
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderSpecificityOrdering) {
    // When quality is the same, more specific should come first
    auto types = negotiator.parseAcceptHeader("*/*;q=0.8, text/*;q=0.8, text/html;q=0.8");

    // All have same quality, but should be sorted by specificity
    EXPECT_EQ(types[0].type, "text/html");
    EXPECT_EQ(types[0].specificity, 3);

    EXPECT_EQ(types[1].type, "text/*");
    EXPECT_EQ(types[1].specificity, 2);

    EXPECT_EQ(types[2].type, "*/*");
    EXPECT_EQ(types[2].specificity, 1);
}

// Test finding variants - no variants
TEST_F(ContentNegotiatorTest, FindVariantsNone) {
    auto variants = negotiator.findVariants(test_dir + "/nonexistent");
    EXPECT_TRUE(variants.empty());
}

// Test finding variants - single variant
TEST_F(ContentNegotiatorTest, FindVariantsSingle) {
    createTestFile(test_dir + "/data.json", "{\"test\": true}");

    auto variants = negotiator.findVariants(test_dir + "/data");
    EXPECT_EQ(variants.size(), 1u);
    EXPECT_EQ(variants[test_dir + "/data.json"], "application/json");
}

// Test finding variants - multiple variants
TEST_F(ContentNegotiatorTest, FindVariantsMultiple) {
    createTestFile(test_dir + "/api.json", "{}");
    createTestFile(test_dir + "/api.xml", "<root/>");
    createTestFile(test_dir + "/api.html", "<html></html>");

    auto variants = negotiator.findVariants(test_dir + "/api");
    EXPECT_EQ(variants.size(), 3u);
    EXPECT_EQ(variants[test_dir + "/api.json"], "application/json");
    EXPECT_EQ(variants[test_dir + "/api.xml"], "application/xml");
    EXPECT_EQ(variants[test_dir + "/api.html"], "text/html");
}

// Test content type scoring - exact match
TEST_F(ContentNegotiatorTest, ScoreContentTypeExactMatch) {
    auto preferences = negotiator.parseAcceptHeader("text/html, application/json;q=0.9");

    double score_html = negotiator.scoreContentType("text/html", preferences);
    EXPECT_DOUBLE_EQ(score_html, 1.0);

    double score_json = negotiator.scoreContentType("application/json", preferences);
    EXPECT_DOUBLE_EQ(score_json, 0.9);
}

// Test content type scoring - wildcard match
TEST_F(ContentNegotiatorTest, ScoreContentTypeWildcardMatch) {
    auto preferences = negotiator.parseAcceptHeader("text/*, application/json;q=0.5");

    double score_html = negotiator.scoreContentType("text/html", preferences);
    EXPECT_GT(score_html, 0.9); // Should match text/* with q=1.0

    double score_json = negotiator.scoreContentType("application/json", preferences);
    EXPECT_DOUBLE_EQ(score_json, 0.5);
}

// Test content type scoring - no match
TEST_F(ContentNegotiatorTest, ScoreContentTypeNoMatch) {
    auto preferences = negotiator.parseAcceptHeader("text/html");

    double score = negotiator.scoreContentType("application/json", preferences);
    EXPECT_DOUBLE_EQ(score, 0.0);
}

// Test content type scoring - */* fallback
TEST_F(ContentNegotiatorTest, ScoreContentTypeStarStarFallback) {
    auto preferences = negotiator.parseAcceptHeader("text/html, */*;q=0.1");

    double score = negotiator.scoreContentType("application/pdf", preferences);
    EXPECT_GT(score, 0.0); // Should match */*
    EXPECT_LT(score, 0.2);
}

// Test selecting best match - prefer JSON
TEST_F(ContentNegotiatorTest, SelectBestMatchPreferJSON) {
    createTestFile(test_dir + "/data.json", "{}");
    createTestFile(test_dir + "/data.xml", "<root/>");
    createTestFile(test_dir + "/data.html", "<html></html>");

    std::string best =
        negotiator.selectBestMatch(test_dir + "/data", "application/json, text/html;q=0.9");

    EXPECT_EQ(best, test_dir + "/data.json");
}

// Test selecting best match - prefer HTML
TEST_F(ContentNegotiatorTest, SelectBestMatchPreferHTML) {
    createTestFile(test_dir + "/page.json", "{}");
    createTestFile(test_dir + "/page.html", "<html></html>");

    std::string best =
        negotiator.selectBestMatch(test_dir + "/page", "text/html, application/json;q=0.5");

    EXPECT_EQ(best, test_dir + "/page.html");
}

// Test selecting best match - no acceptable variant
TEST_F(ContentNegotiatorTest, SelectBestMatchNoAcceptable) {
    createTestFile(test_dir + "/data.json", "{}");

    std::string best = negotiator.selectBestMatch(test_dir + "/data", "text/html");

    EXPECT_TRUE(best.empty());
}

// Test selecting best match - no variants
TEST_F(ContentNegotiatorTest, SelectBestMatchNoVariants) {
    std::string best =
        negotiator.selectBestMatch(test_dir + "/nonexistent", "text/html, application/json");

    EXPECT_TRUE(best.empty());
}

// Test selecting best match - wildcard accepts anything
TEST_F(ContentNegotiatorTest, SelectBestMatchWildcard) {
    createTestFile(test_dir + "/file.pdf", "PDF content");

    std::string best = negotiator.selectBestMatch(test_dir + "/file", "*/*");

    EXPECT_EQ(best, test_dir + "/file.pdf");
}

// Test complex Accept header
TEST_F(ContentNegotiatorTest, SelectBestMatchComplexAccept) {
    createTestFile(test_dir + "/resource.json", "{}");
    createTestFile(test_dir + "/resource.xml", "<root/>");
    createTestFile(test_dir + "/resource.html", "<html></html>");
    createTestFile(test_dir + "/resource.txt", "text");

    // Complex Accept header favoring HTML, then JSON, then anything
    std::string best = negotiator.selectBestMatch(
        test_dir + "/resource", "text/html;q=1.0, application/json;q=0.9, */*;q=0.1");

    EXPECT_EQ(best, test_dir + "/resource.html");
}

// Test quality value edge cases
TEST_F(ContentNegotiatorTest, ParseAcceptHeaderQualityEdgeCases) {
    // Quality = 0 should be treated as not acceptable
    auto types = negotiator.parseAcceptHeader("text/html;q=0.0, application/json;q=1.0");
    EXPECT_EQ(types.size(), 2u);

    // Quality > 1 should be clamped to 1.0
    types = negotiator.parseAcceptHeader("text/html;q=1.5");
    EXPECT_DOUBLE_EQ(types[0].quality, 1.0);

    // Negative quality should be clamped to 0.0
    types = negotiator.parseAcceptHeader("text/html;q=-0.5");
    EXPECT_DOUBLE_EQ(types[0].quality, 0.0);
}

// Test case insensitivity of media types
TEST_F(ContentNegotiatorTest, MediaTypeMatchingCaseInsensitive) {
    MediaType type("Text/HTML", 1.0);
    EXPECT_TRUE(type.matches("text/html"));
    EXPECT_TRUE(type.matches("TEXT/HTML"));
    EXPECT_TRUE(type.matches("Text/Html"));
}

// Test MIME type detection
TEST_F(ContentNegotiatorTest, GetMimeTypeCommonTypes) {
    createTestFile(test_dir + "/test.json", "{}");
    createTestFile(test_dir + "/test.html", "<html></html>");
    createTestFile(test_dir + "/test.xml", "<root/>");
    createTestFile(test_dir + "/test.txt", "text");
    createTestFile(test_dir + "/test.pdf", "PDF");
    createTestFile(test_dir + "/test.png", "PNG");
    createTestFile(test_dir + "/test.jpg", "JPG");

    auto variants = negotiator.findVariants(test_dir + "/test");

    EXPECT_EQ(variants[test_dir + "/test.json"], "application/json");
    EXPECT_EQ(variants[test_dir + "/test.html"], "text/html");
    EXPECT_EQ(variants[test_dir + "/test.xml"], "application/xml");
    EXPECT_EQ(variants[test_dir + "/test.txt"], "text/plain");
    EXPECT_EQ(variants[test_dir + "/test.pdf"], "application/pdf");
    EXPECT_EQ(variants[test_dir + "/test.png"], "image/png");
    EXPECT_EQ(variants[test_dir + "/test.jpg"], "image/jpeg");
}
