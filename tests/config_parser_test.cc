#include "config.h"
#include "config_parser.h"
#include "gtest/gtest.h"
#include <fstream>

class NginxConfigParserTest : public ::testing::Test {
protected:
  NginxConfigParser parser;
  NginxConfig config;
};

// Fixture for testing
class NginxConfigParserTestFixture : public ::testing::Test {
protected:
  NginxConfigParser parser;
  NginxConfig out_config;
  std::string filename = "fixture_config";

  void WriteConfigFile(const std::string &content) {
    std::ofstream config_file(filename);
    config_file << content;
    config_file.close();
  }

  void TearDown() override { remove(filename.c_str()); }
};

// =====================
//   BASIC TESTS
// =====================

// Test that an empty block is accepted.
TEST(NginxConfigParserBlockTest, ValidEmptyBlock) {
  // "server { }" should be valid: the 'server' directive has a child block that
  // is empty.
  std::istringstream config_stream("server { }");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
  // The top-level config should have one statement ("server")
  ASSERT_EQ(config.statements_.size(), 1);
  auto statement = config.statements_[0];
  EXPECT_EQ(statement->tokens_.size(), 1);
  EXPECT_EQ(statement->tokens_[0], "server");
  // The "server" statement should have a child block, which should be empty.
  ASSERT_NE(statement->child_block_, nullptr);
  EXPECT_TRUE(statement->child_block_->statements_.empty());
}

// Test that a block with a valid non-empty statement is accepted.
TEST(NginxConfigParserBlockTest, ValidNonEmptyBlock) {
  // "server { listen 80; }" should be valid.
  std::istringstream config_stream("server { listen 80; }");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
  // Top-level statement "server"
  ASSERT_EQ(config.statements_.size(), 1);
  auto server_statement = config.statements_[0];
  EXPECT_EQ(server_statement->tokens_.size(), 1);
  EXPECT_EQ(server_statement->tokens_[0], "server");
  // The server directive should have a child block.
  ASSERT_NE(server_statement->child_block_, nullptr);
  // The child block should contain one statement ("listen 80;")
  ASSERT_EQ(server_statement->child_block_->statements_.size(), 1);
  auto listen_statement = server_statement->child_block_->statements_[0];
  // Depending on your tokenization, you should get two tokens: "listen" and
  // "80"
  ASSERT_GE(listen_statement->tokens_.size(), 2);
  EXPECT_EQ(listen_statement->tokens_[0], "listen");
  EXPECT_EQ(listen_statement->tokens_[1], "80");
}

// Test that a value with special chars is accepted
TEST(NginxConfigParserBlockTest, SpecialChars) {
  std::istringstream config_stream("server_name ~^example\\.com$;");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
}

// =====================
//   BRACKET TESTS
// =====================

// Test that we cannot have a terminating bracket } without a corresponding
// start bracket
TEST_F(NginxConfigParserTestFixture, MissingStartBracket) {
  WriteConfigFile(R"(
    server 
      listen 80;
      server_name foo.com;
      root /home//ubuntu/sites/foo/
    }
  )");

  // expect false
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Test that there can correctly be two ending brackets when there are nested
// blocks
TEST_F(NginxConfigParserTestFixture, DoubleEndBrackets) {
  WriteConfigFile(R"(
    server {
      listen   80;
      server_name foo.com;
      root /home/ubuntu/sites/foo/;
      location /one {
        goo car;
      }
    }
  )");

  // expect success
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_TRUE(success);
}

// Upon encountering a start, Parse does not yet require a corresponding end
// bracket
TEST_F(NginxConfigParserTestFixture, UnfinishedBrackets) {
  WriteConfigFile(R"(
    server {
      listen 80;
      server_name foo.com;
      root /home/ubuntu/sites/foo/;
    
  )");

  // expect failure
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// expect success when there are multiple server names
TEST_F(NginxConfigParserTestFixture, MultipleValues) {
  WriteConfigFile(R"(
    server {
      listen 80;
      server_name foo.com bar.com;
      root /home//ubuntu/sites/foo/;
      location / {
        proxy_pass http://backend;
      }
    }
  )");

  // expect success
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_TRUE(success);
}

// expect success when there are deeply nested brackets
TEST_F(NginxConfigParserTestFixture, DeeplyNested) {
  WriteConfigFile(R"(
    http {
      server {
        location / {
          if ($cond) { proxy_pass http://backend; }
        }
      }
    }
  )");

  // expect success
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_TRUE(success);
}

// =====================
//   QUOTE TESTS
// =====================

// Verify Double Quote Rejection
TEST_F(NginxConfigParserTestFixture, DoubleQuoteError) {
  WriteConfigFile(R"(
    server {
      listen 80;
      server_name "foo".com;
      root /home//ubuntu/sites/foo/;
      location / {
        proxy_pass http://backend;
      }
    }
  )");
  bool failure = parser.Parse(filename.c_str(), &out_config);
  EXPECT_FALSE(failure);
}

// Verify Single Quote Rejection
TEST(NginxConfigParserTest, SingleQuoteRejection) {
  // Simulates a server_name directive using single quotes.
  std::istringstream config_stream("server_name 'example.com';");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_FALSE(parser.Parse(&config_stream, &config));
}

// Test that quoted strings are not supported
TEST(NginxConfigParserTest, QuotedStringsNotSupported) {
  std::istringstream config_stream("server_name \"example.com\";");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_FALSE(parser.Parse(&config_stream, &config));
}

// =====================
//   SEMICOLON TESTS
// =====================

// Test that a non-empty block without proper termination fails.
// For example, a block that misses a semicolon after a statement should be
// rejected.
TEST(NginxConfigParserBlockTest, InvalidBlockMissingSemicolon) {
  // "server { listen 80 }" is missing the semicolon that terminates the "listen
  // 80" directive.
  std::istringstream config_stream("server { listen 80 }");
  NginxConfig config;
  NginxConfigParser parser;

  // The parser should fail because the block is non-empty and its last token
  // isn't a statement end.
  EXPECT_FALSE(parser.Parse(&config_stream, &config));
}

// Tests that multi-line blocks ending in a non terminated line fails
TEST_F(NginxConfigParserTestFixture, MissingSemicolonEnd) {
  WriteConfigFile(R"(
    server {
      listen 80;
      server_name foo.com;
      root /home/ubuntu/sites/foo/
    }
  )");

  // expect a false result due unterminated line
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// Tests that multi line blocks with an unproperly terminated line between two
// properly terminated lines fails
TEST_F(NginxConfigParserTestFixture, MissingSemicolonBetween) {
  WriteConfigFile(R"(
    server {
      listen 80;
      server_name foo.com
      root /home/ubuntu/sites/foo/;
    }
  )");

  // expect a false result due unterminated line
  bool success = parser.Parse(filename.c_str(), &out_config);
  EXPECT_FALSE(success);
}

// =====================
//   CONFIG FORMAT TESTS
// =====================

// Test valid location format
TEST(NginxConfigParserTest, ValidLocationFormat) {
  std::istringstream config_stream("location /echo EchoHandler {}");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
  ASSERT_EQ(config.statements_.size(), 1);
  auto statement = config.statements_[0];
  EXPECT_EQ(statement->tokens_.size(), 3);
  EXPECT_EQ(statement->tokens_[0], "location");
  EXPECT_EQ(statement->tokens_[1], "/echo");
  EXPECT_EQ(statement->tokens_[2], "EchoHandler");
}

// Test location with arguments
TEST(NginxConfigParserTest, LocationWithArguments) {
  std::istringstream config_stream(R"(
    location /static StaticHandler {
      root ./public;
    }
  )");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
  ASSERT_EQ(config.statements_.size(), 1);
  auto statement = config.statements_[0];
  ASSERT_NE(statement->child_block_, nullptr);
  ASSERT_EQ(statement->child_block_->statements_.size(), 1);
  auto arg_statement = statement->child_block_->statements_[0];
  EXPECT_EQ(arg_statement->tokens_.size(), 2);
  EXPECT_EQ(arg_statement->tokens_[0], "root");
  EXPECT_EQ(arg_statement->tokens_[1], "./public");
}

// Test port declaration still works
TEST(NginxConfigParserTest, PortDeclarationStillWorks) {
  std::istringstream config_stream("port 8080;");
  NginxConfig config;
  NginxConfigParser parser;

  EXPECT_TRUE(parser.Parse(&config_stream, &config));
  ASSERT_EQ(config.statements_.size(), 1);
  auto statement = config.statements_[0];
  EXPECT_EQ(statement->tokens_.size(), 2);
  EXPECT_EQ(statement->tokens_[0], "port");
  EXPECT_EQ(statement->tokens_[1], "8080");
}
