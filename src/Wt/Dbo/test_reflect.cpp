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
#include <Wt/Dbo/session/Query.h>
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

    // Test 10: New operators (ilike, any)
    {
        using namespace Wt::Dbo;

        auto q1 = ilike<^^UserWithFk::name>(std::string{"sYlVaIn%"});
        assert(is_ilike_predicate_v<decltype(q1)>);
        assert(q1.pattern == "sYlVaIn%");
        assert(!q1.negated);

        auto q2 = notIlike<^^UserWithFk::name>("foo%");
        assert(q2.negated);

        auto q3 = any<^^UserWithFk::id>(42LL);
        assert(is_any_predicate_v<decltype(q3)>);
        assert(q3.value == 42LL);

        auto q4 = (q1 && q2) || q3;
        assert(is_or_predicate_v<decltype(q4)>);
    }

    // Test 11: Custom operators (%, ^, >>)
    {
        using namespace Wt::Dbo;

        auto q1 = col<^^UserWithFk::name> % std::string{"%Test%"};
        assert(is_like_predicate_v<decltype(q1)>);
        assert(q1.pattern == "%Test%");

        auto q2 = col<^^UserWithFk::name> ^ std::string{"%Test%"};
        assert(is_ilike_predicate_v<decltype(q2)>);
        assert(q2.pattern == "%Test%");

        auto q3 = col<^^UserWithFk::group> >> 99LL;
        assert(is_any_predicate_v<decltype(q3)>);
        assert(q3.value == 99LL);
    }

    // Test 12: Typed Subqueries (compile-time type verification)
    {
        using namespace Wt::Dbo;

        // Verify InSubqueryPredicate trait detection at compile time
        using SubqType = Query<long long>;
        using PredType = InSubqueryPredicate<^^UserWithFk::group, SubqType, false>;
        static_assert(is_in_subquery_predicate_v<PredType>,
                      "InSubqueryPredicate must satisfy trait");
        static_assert(!PredType::negated, "non-negated");
        static_assert(is_typed_predicate_v<PredType>,
                      "InSubqueryPredicate must be a TypedPredicate");

        using NegPredType = InSubqueryPredicate<^^UserWithFk::group, SubqType, true>;
        static_assert(is_in_subquery_predicate_v<NegPredType>);
        static_assert(NegPredType::negated, "negated variant");

        // Verify it composes with && and ||
        using CmpType = ComparePredicate<^^UserWithFk::id, long long, CompareOp::Eq>;
        using AndType = AndPredicate<CmpType, PredType>;
        static_assert(is_and_predicate_v<AndType>);
        static_assert(is_typed_predicate_v<AndType>);

        std::printf("Test 12: InSubqueryPredicate static_asserts passed\n");
    }

    // Test 13: Eager loading types (WithSpec, WithResult, QueryWith)
    {
        using namespace Wt::Dbo;

        // WithSpec carries a member reflection + target type
        using Spec1 = WithSpec<^^UserWithFk::group, Group>;
        static_assert(Spec1::member == ^^UserWithFk::group);
        static_assert(std::is_same_v<Spec1::target_type, Group>);

        // WithResult stores parents + per-relation maps
        using WR = WithResult<UserWithFk, long long, Group>;
        static_assert(std::is_same_v<
            decltype(std::declval<WR>().entities),
            std::vector<UserWithFk>>);

        // QueryWith carries specs as template params
        using QW = QueryWith<UserWithFk, Spec1>;
        static_assert(std::is_base_of_v<Query<UserWithFk>, QW>,
                      "QueryWith must inherit from Query");

        // Chaining .with<>() produces expanded QueryWith
        using Spec2 = WithSpec<^^UserWithFk::name, TestVisibility>;
        using QW2 = QueryWith<UserWithFk, Spec1, Spec2>;
        static_assert(std::is_base_of_v<Query<UserWithFk>, QW2>);

        std::printf("Test 13: Eager loading types static_asserts passed\n");
    }

    // Test 14: JSONB query types
    {
        using namespace Wt::Dbo;

        // col<^^M>["key"] produces JsonColumnRef<M, 1>
        auto jref = col<^^UserWithFk::name>["role"];
        static_assert(is_json_column_ref_v<decltype(jref)>);
        static_assert(decltype(jref)::path_len == 1);
        assert(jref.path[0] == "role");

        // Chaining: ["address"]["city"] produces JsonColumnRef<M, 2>
        auto jref2 = col<^^UserWithFk::name>["address"]["city"];
        static_assert(is_json_column_ref_v<decltype(jref2)>);
        static_assert(decltype(jref2)::path_len == 2);
        assert(jref2.path[0] == "address");
        assert(jref2.path[1] == "city");

        // Comparison produces JsonPathPredicate
        auto pred = col<^^UserWithFk::name>["role"] == std::string{"admin"};
        static_assert(is_json_path_predicate_v<decltype(pred)>);
        static_assert(is_typed_predicate_v<decltype(pred)>);
        assert(pred.value == "admin");
        assert(pred.path[0] == "role");

        auto likePred = col<^^UserWithFk::name>["address"]["city"] % std::string{"Par%"};
        static_assert(is_json_path_like_predicate_v<decltype(likePred)>);
        static_assert(is_typed_predicate_v<decltype(likePred)>);
        assert(likePred.pattern == "Par%");
        assert(likePred.path[0] == "address");
        assert(likePred.path[1] == "city");

        // Can compose with other predicates
        auto combined = pred && (col<^^UserWithFk::id> == 42LL);
        static_assert(is_and_predicate_v<decltype(combined)>);

        std::printf("Test 14: JSONB query types static_asserts passed\n");
    }

    std::printf("All tests passed!\n");
    return 0;
}
