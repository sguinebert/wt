// Compile-time test for Phase 5 value-oriented API
// Build: /opt/clang-p2996/bin/clang++ -std=c++26 -freflection -fexpansion-statements
//        -stdlib=libc++ -I src -I src/Wt/cuehttp -I build
//        -I /usr/local/include -I /usr/local/lib/cmake/Boost-1.88.0/../../include
//        src/Wt/Dbo/test_reflect.cpp -o /tmp/test_reflect && /tmp/test_reflect

#include <Wt/Dbo/reflect/Meta.h>
#include <Wt/Dbo/reflect/Traits.h>
#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/reflect/Json.h>
#include <Wt/Dbo/core/fk.h>
#include <string>
#include <cassert>
#include <cstdio>

// ===================================================================
// Test types
// ===================================================================

struct TestVisibility {
public:
    int pub_field = 0;
    std::string pub_name;

protected:
    int prot_field = 0;

private:
    int priv_secret = 42;
    std::string priv_data;
};

struct TestOptOut {
public:
    int keep_me = 0;
    int exclude_me = 0;
    std::string also_keep;
};

template<> struct Wt::Dbo::dbo_meta<TestOptOut> {
    static consteval Wt::Dbo::TableOpts table() { return {}; }

    static consteval Wt::Dbo::FieldOpts field_opts(std::meta::info m) {
        if (m == ^^TestOptOut::exclude_me) return {.excluded = true};
        return {};
    }
    static consteval Wt::Dbo::BelongsToOpts belongs_to_opts(std::meta::info) { return {}; }
};

struct TestColumnRename {
public:
    std::string email;
    int age = 0;
};

template<> struct Wt::Dbo::dbo_meta<TestColumnRename> {
    static consteval Wt::Dbo::TableOpts table() { return {}; }

    static consteval Wt::Dbo::FieldOpts field_opts(std::meta::info m) {
        if (m == ^^TestColumnRename::email) return {.column = "email_address", .size = 255};
        return {};
    }
    static consteval Wt::Dbo::BelongsToOpts belongs_to_opts(std::meta::info) { return {}; }
};

// FK test types
struct Group {
public:
    long long id = 0;
    std::string name;
};

struct UserWithFk {
public:
    long long id = 0;
    std::string name;
    Wt::Dbo::fk<Group> group;
};

// ===================================================================
// Op functors for the new protocol (value + foreign_key only)
// ===================================================================

struct CounterOp {
    int value_count = 0;
    int fk_count = 0;

    template<class V>
    void value(V&, std::string_view, Wt::Dbo::FieldOpts) { ++value_count; }

    template<class Target>
    void foreign_key(Wt::Dbo::fk<Target>&, Wt::Dbo::BelongsToOpts) { ++fk_count; }
};

struct ConstCounterOp {
    int value_count = 0;
    int fk_count = 0;

    template<class V>
    void value(const V&, std::string_view, Wt::Dbo::FieldOpts) { ++value_count; }

    template<class Target>
    void foreign_key(const Wt::Dbo::fk<Target>&, Wt::Dbo::BelongsToOpts) { ++fk_count; }
};

struct ColCollector {
    std::string_view cols[10];
    int count = 0;

    template<class V>
    void value(V&, std::string_view col, Wt::Dbo::FieldOpts) {
        cols[count++] = col;
    }

    template<class Target>
    void foreign_key(Wt::Dbo::fk<Target>&, Wt::Dbo::BelongsToOpts) {}
};

// Static Op (no object reference)
struct StaticCounterOp {
    int value_count = 0;
    int fk_count = 0;

    template<class V>
    void value(std::string_view, Wt::Dbo::FieldOpts) { ++value_count; }

    template<class Target>
    void foreign_key(Wt::Dbo::BelongsToOpts) { ++fk_count; }
};

// ===================================================================
// main
// ===================================================================

int main() {
    using namespace Wt::Dbo::Reflect;

    // Test 1: Visibility — private members excluded
    {
        TestVisibility obj;
        CounterOp counter;
        for_each_field(obj, counter);
        assert(counter.value_count == 3 && "Should see 3 value fields (2 public + 1 protected)");
        assert(counter.fk_count == 0 && "No FK fields");
    }

    // Test 2: Opt-out via dbo_meta
    {
        TestOptOut obj;
        CounterOp counter;
        for_each_field(obj, counter);
        assert(counter.value_count == 2 && "Should see 2 fields (exclude_me opted out)");
    }

    // Test 3: Column rename
    {
        TestColumnRename obj;
        ColCollector coll;
        for_each_field(obj, coll);
        assert(coll.count == 2);
        assert(coll.cols[0] == "email_address" && "email should be renamed to email_address");
        assert(coll.cols[1] == "age" && "age should keep its member name");
    }

    // Test 4: FK detection
    {
        UserWithFk obj;
        CounterOp counter;
        for_each_field(obj, counter);
        assert(counter.value_count == 2 && "id + name = 2 value fields");
        assert(counter.fk_count == 1 && "group = 1 FK field");
    }

    // Test 5: Const overload
    {
        const UserWithFk obj{};
        ConstCounterOp counter;
        for_each_field(obj, counter);
        assert(counter.value_count == 2);
        assert(counter.fk_count == 1);
    }

    // Test 6: Static iteration (no object)
    {
        StaticCounterOp counter;
        for_each_field_static<UserWithFk>(counter);
        assert(counter.value_count == 2);
        assert(counter.fk_count == 1);
    }

    // Test 7: JSON serialization
    {
        TestColumnRename obj;
        obj.email = "test@example.com";
        obj.age = 30;
        std::string json = Wt::Dbo::Reflect::dbo_to_json(obj);
        std::printf("Test 7 JSON: %s\n", json.c_str());
        assert(json.find("\"email_address\"") != std::string::npos);
        assert(json.find("\"age\"") != std::string::npos);
        assert(json.find("test@example.com") != std::string::npos);
    }

    // Test 8: JSON opt-out field excluded
    {
        TestOptOut obj;
        obj.keep_me = 7;
        obj.exclude_me = 999;
        obj.also_keep = "hello";
        std::string json = Wt::Dbo::Reflect::dbo_to_json(obj);
        std::printf("Test 8 JSON: %s\n", json.c_str());
        assert(json.find("\"keep_me\"") != std::string::npos);
        assert(json.find("\"also_keep\"") != std::string::npos);
        assert(json.find("\"exclude_me\"") == std::string::npos);
    }

    // Test 9: JSON round-trip
    {
        TestColumnRename obj;
        obj.email = "original";
        obj.age = 0;
        std::string json = R"({"email_address":"new@test.com","age":42})";
        bool ok = Wt::Dbo::Reflect::dbo_from_json(obj, json);
        std::printf("Test 9: ok=%d email=%s age=%d\n", ok, obj.email.c_str(), obj.age);
        assert(ok);
        assert(obj.email == "new@test.com");
        assert(obj.age == 42);
    }

    std::printf("All tests passed!\n");
    return 0;
}
