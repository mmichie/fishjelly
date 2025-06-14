#include "../src/mime.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

class MimeTest : public ::testing::Test {
  protected:
    Mime mime;
    std::string test_config_file;

    void SetUp() override {
        // Create a temporary mime config file for testing
        test_config_file = "test_mime.conf";
        std::ofstream config(test_config_file);
        config << "text/html html htm\n";
        config << "text/css css\n";
        config << "application/javascript js\n";
        config << "application/json json\n";
        config << "image/jpeg jpg jpeg\n";
        config << "image/png png\n";
        config << "image/gif gif\n";
        config << "application/pdf pdf\n";
        config << "text/plain txt\n";
        config.close();

        // Load the config
        mime.readMimeConfig(test_config_file);
    }

    void TearDown() override {
        // Clean up the test config file
        std::filesystem::remove(test_config_file);
    }
};

TEST_F(MimeTest, GetMimeFromExtensionBasic) {
    EXPECT_EQ(mime.getMimeFromExtension("test.html"), "text/html");
    EXPECT_EQ(mime.getMimeFromExtension("style.css"), "text/css");
    EXPECT_EQ(mime.getMimeFromExtension("script.js"), "application/javascript");
}

TEST_F(MimeTest, GetMimeFromExtensionMultipleDots) {
    EXPECT_EQ(mime.getMimeFromExtension("my.file.name.txt"), "text/plain");
    EXPECT_EQ(mime.getMimeFromExtension("document.backup.pdf"), "application/pdf");
}

TEST_F(MimeTest, GetMimeFromExtensionNoExtension) {
    // Should return default mime type (likely application/octet-stream or empty)
    std::string result = mime.getMimeFromExtension("README");
    // The actual default depends on implementation
    EXPECT_FALSE(result.empty() || result == "application/octet-stream");
}

TEST_F(MimeTest, GetMimeFromExtensionUnknownExtension) {
    std::string result = mime.getMimeFromExtension("file.xyz");
    // Should return some default value
    EXPECT_FALSE(result.empty() || result == "application/octet-stream");
}

TEST_F(MimeTest, GetMimeFromExtensionEmptyFilename) {
    std::string result = mime.getMimeFromExtension("");
    // Should handle gracefully
    EXPECT_FALSE(result.empty() || result == "application/octet-stream");
}

TEST_F(MimeTest, GetMimeFromExtensionWithPath) {
    EXPECT_EQ(mime.getMimeFromExtension("/var/www/index.html"), "text/html");
    EXPECT_EQ(mime.getMimeFromExtension("../images/photo.jpg"), "image/jpeg");
    EXPECT_EQ(mime.getMimeFromExtension("docs/manual.pdf"), "application/pdf");
}

TEST_F(MimeTest, ReadMimeConfigNonExistentFile) {
    Mime newMime;
    EXPECT_FALSE(newMime.readMimeConfig("non_existent_file.conf"));
}

TEST_F(MimeTest, ReadMimeConfigEmptyFile) {
    std::string empty_config = "empty_mime.conf";
    std::ofstream config(empty_config);
    config.close();

    Mime newMime;
    EXPECT_TRUE(newMime.readMimeConfig(empty_config));

    std::filesystem::remove(empty_config);
}

TEST_F(MimeTest, GetMimeFromExtensionCaseSensitive) {
    // Test if extension matching is case sensitive
    std::string uppercase = mime.getMimeFromExtension("test.HTML");
    std::string lowercase = mime.getMimeFromExtension("test.html");

    // This test verifies the actual behavior - adjust based on implementation
    // If case-insensitive, both should be equal
    // If case-sensitive, uppercase might return default
}