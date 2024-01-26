#include "spdlog/spdlog.h"
#include "ylt/struct_pack.hpp"

void HelloWorld() 
{
    {
        // spdlogger example
        spdlog::info("Hello, World!");
    }

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
}


