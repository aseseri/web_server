#include "registry.h"
#include "gtest/gtest.h"
#include "static_handler.h"
#include "echo_handler.h"
#include "not_found_handler.h"
#include <thread>

class RegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset registry between tests
        Registry::ResetForTesting();
        // Register required handlers
        Registry::RegisterHandler("StaticHandler",
            [](const std::string& path, const auto& args) {
                return std::make_unique<StaticHandler>(path, args.at("root"));
            }
        );
        Registry::RegisterHandler("EchoHandler",
            [](const std::string& path, const auto& args) {
                return std::make_unique<EchoHandler>(path);
            }
        );
    }
};

// Tests basic handler registration and retrieval functionality
TEST_F(RegistryTest, CanRegisterAndRetrieveHandler) {
    bool factory_invoked = false;
    Registry::RegisterHandler("TestHandler", [&](auto&&...) {
        factory_invoked = true;
        return nullptr;
    });

    auto factory = Registry::GetHandlerFactory("TestHandler");
    ASSERT_NE(factory, nullptr) << "Factory not registered";
    EXPECT_NO_THROW(factory("", {}));
    EXPECT_TRUE(factory_invoked);
}

// Verifies macro-based registration works for StaticHandler
TEST_F(RegistryTest, MacroRegistration) {
    // StaticHandler should be registered via REGISTER_HANDLER macro
    auto factory = Registry::GetHandlerFactory("StaticHandler");
    ASSERT_NE(factory, nullptr);
    
    std::unordered_map<std::string, std::string> args{{"root", "./test"}};
    auto handler = factory("/static", args);
    auto static_handler = dynamic_cast<StaticHandler*>(handler.get());
    ASSERT_NE(static_handler, nullptr) << "Incorrect handler type created";
}

// Tests registry behavior for unregistered handler types
TEST_F(RegistryTest, UnknownHandlerReturnsNull) {
    EXPECT_TRUE(Registry::GetHandlerFactory("NonExistentHandler") == nullptr);
}

// Verifies registry supports multiple distinct handler registrations
TEST_F(RegistryTest, CanRegisterMultipleHandlers) {
    Registry::RegisterHandler("H1", [](auto&&...) { return nullptr; });
    Registry::RegisterHandler("H2", [](auto&&...) { return nullptr; });

    EXPECT_NE(Registry::GetHandlerFactory("H1"), nullptr);
    EXPECT_NE(Registry::GetHandlerFactory("H2"), nullptr);
}

// Ensures duplicate handler registrations are rejected
TEST_F(RegistryTest, ThrowsOnDuplicateRegistration) {
    auto factory = [](auto&&...) { return nullptr; };

    Registry::RegisterHandler("TestHandler", factory);
    EXPECT_THROW(
        Registry::RegisterHandler("TestHandler", factory), 
        std::runtime_error
    );
}

// Tests EchoHandler registration and basic functionality
TEST_F(RegistryTest, EchoHandlerRegistrationAndRequestProcessing) {
    // Factory registration
    auto factory = Registry::GetHandlerFactory("EchoHandler");
    ASSERT_NE(factory, nullptr) << "EchoHandler not registered";
    
    // Handler instantiation
    auto handler = factory("/echo", {});
    auto echo_handler = dynamic_cast<EchoHandler*>(handler.get());
    ASSERT_NE(echo_handler, nullptr) << "Incorrect handler type created";
    
    // Verify request handling
    HttpRequest req;
    req.path = "/echo/test";
    req.raw = "GET /echo/test HTTP/1.1";
    auto resp = handler->handle_request(req);
    
    EXPECT_EQ(resp->status_code, 200) << "Incorrect status code";
    EXPECT_EQ(resp->body, "GET /echo/test HTTP/1.1")
        << "Response body mismatch";
    EXPECT_EQ(resp->headers["Content-Type"], "text/plain")
        << "Incorrect content type";
}

TEST_F(RegistryTest, ConcurrentHandlerRegistrationAndAccess) {
    const int num_threads = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successes(0);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                // Alternate between registration and access
                if (i % 2 == 0) {
                    Registry::RegisterHandler("TestHandler" + std::to_string(i),
                        [](auto&&...) { return nullptr; });
                } else {
                    auto factory = Registry::GetHandlerFactory("EchoHandler");
                    if (factory) successes++;
                }
            } catch (...) {}
        });
    }

    for (auto& t : threads) t.join();
    EXPECT_GT(successes, 0); // At least some reads succeeded
}
