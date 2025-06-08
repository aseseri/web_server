#include "path_mutex_provider.h"
#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include <set>

class PathMutexProviderTest : public ::testing::Test {};

// Test that calls for the same path return a reference to the same mutex object
TEST_F(PathMutexProviderTest, SamePathReturnsSameMutex) {
    std::string path_a = "/data/resource1";
    
    std::mutex& mutex1 = PathMutexProvider::get_mutex_for_path(path_a);
    std::mutex& mutex2 = PathMutexProvider::get_mutex_for_path(path_a);
    
    EXPECT_EQ(&mutex1, &mutex2);
}

// Test that calls for different paths return references to different mutex objects
TEST_F(PathMutexProviderTest, DifferentPathsReturnDifferentMutexes) {
    std::string path_a = "/data/resourceA";
    std::string path_b = "/data/resourceB";
    
    std::mutex& mutex_a = PathMutexProvider::get_mutex_for_path(path_a);
    std::mutex& mutex_b = PathMutexProvider::get_mutex_for_path(path_b);
    
    EXPECT_NE(&mutex_a, &mutex_b);
}

// Test that the PathMutexProvider is thread-safe
// This test spawns multiple threads that concurrently request mutexes for various paths
TEST_F(PathMutexProviderTest, ProviderIsThreadSafe) {
    const int num_threads = 20;
    const int num_paths = 5;
    std::vector<std::thread> threads;

    auto worker = [&]() {
        // Each thread will request mutexes for all test paths
        for (int i = 0; i < num_paths; ++i) {
            std::string path = "/data/concurrent/path_" + std::to_string(i);
            // The act of getting the mutex is the critical section we are testing. Lock it briefly to simulate work
            std::lock_guard<std::mutex> lock(PathMutexProvider::get_mutex_for_path(path));
            // A small sleep to increase the chance of thread interleaving.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    // The test passes if it completes without crashing or deadlocking
    SUCCEED();
}
