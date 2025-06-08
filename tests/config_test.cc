#include "config.h"
#include "config_statement.h"
#include "gtest/gtest.h"

class ConfigTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create test configs
    empty_config_ = std::make_unique<NginxConfig>();
    simple_config_ = std::make_unique<NginxConfig>();
    nested_config_ = std::make_unique<NginxConfig>();

    // Setup simple config (port 8080)
    auto statement = std::make_unique<NginxConfigStatement>();
    statement->tokens_ = {"port", "8080"};
    simple_config_->statements_.push_back(std::move(statement));

    // Setup nested config (port 80 inside server block)
    auto server_block = std::make_unique<NginxConfig>();
    auto port_statement = std::make_unique<NginxConfigStatement>();
    port_statement->tokens_ = {"port", "80"};
    server_block->statements_.push_back(std::move(port_statement));

    auto server_statement = std::make_unique<NginxConfigStatement>();
    server_statement->tokens_ = {"server"};
    server_statement->child_block_ = std::move(server_block);
    nested_config_->statements_.push_back(std::move(server_statement));
  }

  std::unique_ptr<NginxConfig> empty_config_;
  std::unique_ptr<NginxConfig> simple_config_;
  std::unique_ptr<NginxConfig> nested_config_;
};

// =====================
//   ToString TESTS
// =====================

// Tests that ToString propery turns an empty config into an empty string
TEST_F(ConfigTest, ToString_EmptyConfig) {
  EXPECT_EQ(empty_config_->ToString(0), "");
}

// Tests that ToString propery turns a single statement config into a single
// statement string
TEST_F(ConfigTest, ToString_SimpleConfig) {
  EXPECT_EQ(simple_config_->ToString(0), "port 8080;\n");
}

// Tests that ToString propery turns a multiple statement config into an multi
// statement string
TEST_F(ConfigTest, ToString_MultipleStatements) {
  auto config = std::make_shared<NginxConfig>();
  auto stmt1 = std::make_shared<NginxConfigStatement>();
  stmt1->tokens_ = {"listen", "8080"};
  auto stmt2 = std::make_shared<NginxConfigStatement>();
  stmt2->tokens_ = {"root", "/var/www"};
  config->statements_.push_back(stmt1);
  config->statements_.push_back(stmt2);

  EXPECT_EQ(config->ToString(0), "listen 8080;\nroot /var/www;\n");
}

// =====================
//   GetPortNumber TESTS
// =====================

// Tests that GetPortNumber returns an error when there is no port in the config
TEST_F(ConfigTest, GetPortNumber_EmptyConfig) {
  EXPECT_EQ(empty_config_->GetPortNumber(), -1);
}

// Tests that GetPortNumber returns the proper port in a simple single statement
// config
TEST_F(ConfigTest, GetPortNumber_SimpleConfig) {
  EXPECT_EQ(simple_config_->GetPortNumber(), 8080);
}

// Tests that GetPortNumber returns the port when there is a nested config
TEST_F(ConfigTest, GetPortNumber_NestedConfig) {
  EXPECT_EQ(nested_config_->GetPortNumber(), 80);
}

// Tests that GetPortNumber returns a failure upon an invalid port being found
TEST_F(ConfigTest, GetPortNumber_InvalidPort) {
  auto config = std::make_shared<NginxConfig>();
  auto statement = std::make_shared<NginxConfigStatement>();
  statement->tokens_ = {"port", "invalid"};
  config->statements_.push_back(statement);

  EXPECT_EQ(config->GetPortNumber(), -1);
}

// Tests that GetPortNumber returns the proper port when there are multiple
// (first port found)
TEST_F(ConfigTest, GetPortNumber_MultiplePorts) {
  auto config = std::make_shared<NginxConfig>();
  auto stmt1 = std::make_shared<NginxConfigStatement>();
  stmt1->tokens_ = {"port", "8080"};
  auto stmt2 = std::make_shared<NginxConfigStatement>();
  stmt2->tokens_ = {"port", "80"};
  config->statements_.push_back(stmt1);
  config->statements_.push_back(stmt2);

  // Should return first valid port found
  EXPECT_EQ(config->GetPortNumber(), 8080);
}

// =====================
//   GetHandlerConfigs TESTS
// =====================

// Tests that GetHandlerConfigs parses no handlers for an empty config
TEST_F(ConfigTest, GetHandlerConfigs_EmptyConfig) {
  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(empty_config_->GetHandlerConfigs(handlers));
  EXPECT_TRUE(handlers.empty());
}

// Tests that GetHandlerConfigs correctly parses a single handler
// (and its path, type, and arguments)
TEST_F(ConfigTest, GetHandlerConfigs_SingleHandler) {
  auto config = std::make_unique<NginxConfig>();

  auto echo_block = std::make_unique<NginxConfig>();
  auto echo_statement = std::make_unique<NginxConfigStatement>();
  echo_statement->tokens_ = {"location", "/echo", "EchoHandler"};
  echo_statement->child_block_ = std::move(echo_block);
  config->statements_.push_back(std::move(echo_statement));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(config->GetHandlerConfigs(handlers));
  ASSERT_EQ(handlers.size(), 1);
  EXPECT_EQ(handlers[0].path, "/echo");
  EXPECT_EQ(handlers[0].type, "EchoHandler");
  EXPECT_TRUE(handlers[0].args.empty());
}

// Tests that GetHandlerConfigs correctly parses a handler with arguments
TEST_F(ConfigTest, GetHandlerConfigs_HandlerWithArgs) {
  auto config = std::make_unique<NginxConfig>();

  auto static_block = std::make_unique<NginxConfig>();
  auto root_statement = std::make_unique<NginxConfigStatement>();
  root_statement->tokens_ = {"root", "./public"};
  static_block->statements_.push_back(std::move(root_statement));

  auto static_statement = std::make_unique<NginxConfigStatement>();
  static_statement->tokens_ = {"location", "/static", "StaticHandler"};
  static_statement->child_block_ = std::move(static_block);
  config->statements_.push_back(std::move(static_statement));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(config->GetHandlerConfigs(handlers));
  ASSERT_EQ(handlers.size(), 1);
  EXPECT_EQ(handlers[0].path, "/static");
  EXPECT_EQ(handlers[0].type, "StaticHandler");
  ASSERT_EQ(handlers[0].args.size(), 1);
  EXPECT_EQ(handlers[0].args["root"], "./public");
}

// Tests that GetHandlerConfigs fails and returns false
// if there are two handlers with the same location
TEST_F(ConfigTest, GetHandlerConfigs_DuplicateLocations) {
  auto config = std::make_unique<NginxConfig>();

  auto echo_block1 = std::make_unique<NginxConfig>();
  auto echo_statement1 = std::make_unique<NginxConfigStatement>();
  echo_statement1->tokens_ = {"location", "/echo", "EchoHandler"};
  echo_statement1->child_block_ = std::move(echo_block1);
  config->statements_.push_back(std::move(echo_statement1));

  auto echo_block2 = std::make_unique<NginxConfig>();
  auto echo_statement2 = std::make_unique<NginxConfigStatement>();
  echo_statement2->tokens_ = {"location", "/echo", "EchoHandlerV2"};
  echo_statement2->child_block_ = std::move(echo_block2);
  config->statements_.push_back(std::move(echo_statement2));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_FALSE(config->GetHandlerConfigs(handlers));
}

// Tests that GetHandlerConfigs enforces prohibited trailing slashes on paths
TEST_F(ConfigTest, GetHandlerConfigs_TrailingSlash) {
  auto config = std::make_unique<NginxConfig>();

  auto echo_block = std::make_unique<NginxConfig>();
  auto echo_statement = std::make_unique<NginxConfigStatement>();
  echo_statement->tokens_ = {"location", "/echo/", "EchoHandler"};
  echo_statement->child_block_ = std::move(echo_block);
  config->statements_.push_back(std::move(echo_statement));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_FALSE(config->GetHandlerConfigs(handlers));
}

// Tests that GetHandlerConfigs can parse multiple handlers in one config
TEST_F(ConfigTest, GetHandlerConfigs_MultipleHandlers) {
  auto config = std::make_unique<NginxConfig>();

  auto echo_block = std::make_unique<NginxConfig>();
  auto echo_statement = std::make_unique<NginxConfigStatement>();
  echo_statement->tokens_ = {"location", "/echo", "EchoHandler"};
  echo_statement->child_block_ = std::move(echo_block);
  config->statements_.push_back(std::move(echo_statement));

  auto static_block = std::make_unique<NginxConfig>();
  auto root_statement = std::make_unique<NginxConfigStatement>();
  root_statement->tokens_ = {"root", "./public"};
  static_block->statements_.push_back(std::move(root_statement));

  auto static_statement = std::make_unique<NginxConfigStatement>();
  static_statement->tokens_ = {"location", "/static", "StaticHandler"};
  static_statement->child_block_ = std::move(static_block);
  config->statements_.push_back(std::move(static_statement));

  std::vector<NginxConfig::HandlerConfig> handlers;
  config->GetHandlerConfigs(handlers);
  ASSERT_EQ(handlers.size(), 2);
}

// Tests that GetHandlerConfigs fails validation when location blocks
// do not have enough tokens (e.g. location server_path handler_name)
TEST_F(ConfigTest, InvalidLocationFormat) {
  auto invalid_location_config_ = std::make_unique<NginxConfig>();
  auto invalid_stmt = std::make_unique<NginxConfigStatement>();
  invalid_stmt->tokens_ = {"location", "/echo"}; // Missing handler
  invalid_stmt->child_block_ = std::make_unique<NginxConfig>();
  invalid_location_config_->statements_.push_back(std::move(invalid_stmt));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_FALSE(invalid_location_config_->GetHandlerConfigs(handlers));
}

// Tests that GetHandlerConfigs fails validation when location blocks have extra tokens
TEST_F(ConfigTest, GetHandlerConfigs_ExtraTokensInLocation) {
  auto config = std::make_unique<NginxConfig>();
  auto invalid_stmt = std::make_unique<NginxConfigStatement>();
  invalid_stmt->tokens_ = {"location", "/echo", "EchoHandler", "ExtraToken"};
  invalid_stmt->child_block_ = std::make_unique<NginxConfig>();
  config->statements_.push_back(std::move(invalid_stmt));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_FALSE(config->GetHandlerConfigs(handlers));
}

// Tests that GetHandlerConfigs parses but ignores invalid handler arguments with missing values
TEST_F(ConfigTest, GetHandlerConfigs_InvalidHandlerArgs) {
  auto config = std::make_unique<NginxConfig>();
  auto static_block = std::make_unique<NginxConfig>();
  // Invalid: "root" has no value
  auto invalid_arg = std::make_unique<NginxConfigStatement>();
  invalid_arg->tokens_ = {"root"};
  static_block->statements_.push_back(std::move(invalid_arg));

  auto static_stmt = std::make_unique<NginxConfigStatement>();
  static_stmt->tokens_ = {"location", "/static", "StaticHandler"};
  static_stmt->child_block_ = std::move(static_block);
  config->statements_.push_back(std::move(static_stmt));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(config->GetHandlerConfigs(handlers));
  EXPECT_TRUE(handlers[0].args.empty());
}

// Tests that GetHandlerConfigs accepts syntactically valid configs with unknown handler types
// (type validation is handled elsewhere in the system)
TEST_F(ConfigTest, GetHandlerConfigs_InvalidHandlerType) {
  auto config = std::make_unique<NginxConfig>();
  auto invalid_block = std::make_unique<NginxConfig>();
  auto invalid_stmt = std::make_unique<NginxConfigStatement>();
  invalid_stmt->tokens_ = {"location", "/test", "UnknownHandler"};
  invalid_stmt->child_block_ = std::move(invalid_block);
  config->statements_.push_back(std::move(invalid_stmt));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(config->GetHandlerConfigs(handlers));
  EXPECT_EQ(handlers.size(), 1);
  EXPECT_EQ(handlers[0].type, "UnknownHandler");
}

// Tests that GetHandlerConfigs correctly parses authentication config
TEST_F(ConfigTest, GetHandlerConfigs_AuthenticationEnabled) {
  auto config = std::make_unique<NginxConfig>();

  auto auth_block = std::make_unique<NginxConfig>();
  auto auth_statement = std::make_unique<NginxConfigStatement>();
  auth_statement->tokens_ = {"requires_auth", "true"};
  auth_block->statements_.push_back(std::move(auth_statement));

  auto location_statement = std::make_unique<NginxConfigStatement>();
  location_statement->tokens_ = {"location", "/auth", "ProtectedHandler"};
  location_statement->child_block_ = std::move(auth_block);
  config->statements_.push_back(std::move(location_statement));

  std::vector<NginxConfig::HandlerConfig> handlers;
  EXPECT_TRUE(config->GetHandlerConfigs(handlers));
  ASSERT_EQ(handlers.size(), 1);
  EXPECT_EQ(handlers[0].path, "/auth");
  EXPECT_EQ(handlers[0].type, "ProtectedHandler");
  EXPECT_TRUE(handlers[0].requires_auth);
}
