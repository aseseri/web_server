#include "mime_types.h"
#include "gtest/gtest.h"


class MimeTypeTest : public ::testing::Test {
protected:
};

// Test that MimeType::LookupByExtension returns the correct MIME type
TEST_F(MimeTypeTest, KnownExtensions) {
  EXPECT_EQ(MimeType::LookupByExtension("file.html"), "text/html");
  EXPECT_EQ(MimeType::LookupByExtension("style.css"), "text/css");
  EXPECT_EQ(MimeType::LookupByExtension("script.js"), "application/javascript");
  EXPECT_EQ(MimeType::LookupByExtension("data.json"), "application/json");
  EXPECT_EQ(MimeType::LookupByExtension("image.jpg"), "image/jpeg");
  EXPECT_EQ(MimeType::LookupByExtension("archive.zip"), "application/zip");
  EXPECT_EQ(MimeType::LookupByExtension("notes.txt"), "text/plain");
}

// Test that MimeType::LookupByExtension returns "application/octet-stream" for
// unknown or no extensions
TEST_F(MimeTypeTest, UnknownOrNoExtensionGivesOctetStream) {
  EXPECT_EQ(MimeType::LookupByExtension("noext"), "application/octet-stream");
  EXPECT_EQ(MimeType::LookupByExtension("weird.ext2"), "application/octet-stream");
}