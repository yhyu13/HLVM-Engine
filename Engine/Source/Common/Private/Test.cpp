#include "Common.h"

/*
    <test suit>
*/
#include <iostream>
#include <vector>
#include <functional>
#include <chrono>

std::vector<std::function<void()>> recorded_test_functions;

// Helper function to create a lambda that runs the test and prints the info
template <typename Func>
std::function<void()> make_test_wrapper(const std::string& name, Func test_function) {
    return [name, test_function]() {
        std::cout << "Running " << name << "..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        
        test_function(); // Run the actual test function
        
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Completed " << name << ". Time elapsed: " << elapsed.count() << " seconds" << std::endl;
    };
}

// Macro to record a test function
#define RECORD(test_function) \
    namespace AutoRegister_##test_function{ \
    struct AutoRegister { \
        AutoRegister() { \
            recorded_test_functions.push_back(make_test_wrapper(#test_function, test_function)); \
        } \
    }; \
    AutoRegister auto_register_##test_function = AutoRegister(); \
    }

/*
    <test method>
*/
void spdlog_test()
{
    spdlog::info("Welcome to spdlog!");
    spdlog::error("Some error message with arg: {}", 1);
    
    spdlog::warn("Easy padding in numbers like {:08d}", 12);
    spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    spdlog::info("Support for floats {:03.2f}", 1.23456);
    spdlog::info("Positional args are {1} {0}..", "too", "supported");
    spdlog::info("{:<30}", "left aligned");
    
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    spdlog::debug("This message should be displayed..");    
    
    // change log pattern
    spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
    
    // Compile time log levels
    // Note that this does not change the current log level, it will only
    // remove (depending on SPDLOG_ACTIVE_LEVEL) the call on the release code.
    SPDLOG_TRACE("Some trace message with param {}", 42);
    SPDLOG_DEBUG("Some debug message");
}
RECORD(spdlog_test);

void yalantinlibs_test()
{
    // Yalantin example
    struct person {
        int64_t id;
        std::string name;
        int age;
        double salary;

        bool operator==(const person& other) const {
            return id == other.id && name == other.name && age == other.age && salary == other.salary;
        }
    };

    person person1{.id = 1, .name = "hello struct pack", .age = 20, .salary = 1024.42};

    // one line code serialize
    auto buffer = struct_pack::serialize(person1);

    // one line code deserialization
    person person2;
    auto ec = struct_pack::deserialize_to(person2, buffer.data(), buffer.size());
    assert(!ec);
    assert(person1 == person2);
}
RECORD(yalantinlibs_test);

int main() 
{
    // Run all registered test functions
    for (auto& test_function : recorded_test_functions) 
    {
        test_function();
    }
    return 0;
}