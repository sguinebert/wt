/*
 * Compile-time test for Wt::Dbo C++26 reflection infrastructure.
 *
 * Verifies that:
 * 1. dbo_meta<C> default and specialized work
 * 2. ReflectTraits classify members correctly
 * 3. ReflectIterators for_each_field dispatches correctly
 *
 * This is a standalone test that only includes the reflection headers
 * (no Session, no SQL, no Boost.Asio) to validate the core mechanism.
 */

#include <iostream>
#include <string>
#include <string_view>
#include <cassert>
#include <vector>
#include <memory>

// Include only the reflection layer — no Session/DB deps
#include <Wt/Dbo/reflect/Meta.h>
#include <Wt/Dbo/reflect/Traits.h>

#ifdef USE_CPP26_REFLECTION

// Minimal stubs for ptr, collection, weak_ptr, junction
// so we don't need the full DBO headers (which pull in Boost.Asio)
namespace Wt { namespace Dbo {

template<class C> class ptr {
    C* p_ = nullptr;
public:
    ptr() = default;
};

template<class C> class weak_ptr {
public:
    weak_ptr() = default;
};

template<class C> class collection {
public:
    collection() = default;
};

template<class C> class junction : public collection<ptr<C>> {
public:
    using collection<ptr<C>>::collection;
};

// Minimal Dbo base
template<class C> class Dbo {
public:
    Dbo() = default;
};

}} // Wt::Dbo

// Now include iterators (they depend on ReflectTraits which depends on fwd decls)
// But we already defined the types above, so we include it after
#include <Wt/Dbo/reflect/Iterators.h>

namespace dbo = Wt::Dbo;
using namespace Wt::Dbo::Reflect;

// ---------------------------------------------------------------------------
// Test model classes — NO persist() needed!
// ---------------------------------------------------------------------------

class Group;
class Post;
class Tag;

class User : public dbo::Dbo<User> {
public:
    std::string name;
    int age = 0;
    dbo::ptr<Group> group;
    dbo::collection<dbo::ptr<Post>> posts;
private:
    int cache_ = 0;  // private → should be excluded
};

class Group : public dbo::Dbo<Group> {
public:
    std::string name;
    dbo::collection<dbo::ptr<User>> users;
};

class Post : public dbo::Dbo<Post> {
public:
    std::string title;
    std::string body;
    dbo::ptr<User> author;
    dbo::junction<Tag> tags;  // ManyToMany
};

class Tag : public dbo::Dbo<Tag> {
public:
    std::string name;
    dbo::junction<Post> posts;
};

// ---------------------------------------------------------------------------
// Customized dbo_meta for User
// ---------------------------------------------------------------------------

namespace Wt { namespace Dbo {

template<>
struct dbo_meta<User> {
    static constexpr TableOpts table{.table_name = "users"};

    static consteval FieldOpts field_opts(std::meta::info m) {
        if (m == ^^User::name) return {.column = "full_name", .size = 128};
        return {};
    }

    static consteval BelongsToOpts belongs_to_opts(std::meta::info m) {
        if (m == ^^User::group) return {.name = "dept"};
        return {};
    }

    static consteval HasManyOpts has_many_opts(std::meta::info m) {
        if (m == ^^User::posts) return {.join_name = "author"};
        return {};
    }

    static consteval HasOneOpts has_one_opts(std::meta::info) { return {}; }
};

template<>
struct dbo_meta<Post> {
    static constexpr TableOpts table{};
    static consteval FieldOpts field_opts(std::meta::info) { return {}; }
    static consteval BelongsToOpts belongs_to_opts(std::meta::info) { return {}; }

    static consteval HasManyOpts has_many_opts(std::meta::info m) {
        if (m == ^^Post::tags) return {.join_name = "post_tags"};
        return {};
    }

    static consteval HasOneOpts has_one_opts(std::meta::info) { return {}; }
};

template<>
struct dbo_meta<Tag> {
    static constexpr TableOpts table{};
    static consteval FieldOpts field_opts(std::meta::info) { return {}; }
    static consteval BelongsToOpts belongs_to_opts(std::meta::info) { return {}; }

    static consteval HasManyOpts has_many_opts(std::meta::info m) {
        if (m == ^^Tag::posts) return {.join_name = "post_tags"};
        return {};
    }

    static consteval HasOneOpts has_one_opts(std::meta::info) { return {}; }
};

}} // namespace Wt::Dbo

// ---------------------------------------------------------------------------
// Static assertions — compile-time type detection
// ---------------------------------------------------------------------------

static_assert(is_dbo_ptr_v<dbo::ptr<Group>>);
static_assert(!is_dbo_ptr_v<std::string>);
static_assert(is_dbo_collection_v<dbo::collection<dbo::ptr<Post>>>);
static_assert(is_dbo_junction_v<dbo::junction<Tag>>);
static_assert(!is_dbo_junction_v<dbo::collection<dbo::ptr<Post>>>);
static_assert(ValueField<std::string>);
static_assert(ValueField<int>);
static_assert(!ValueField<dbo::ptr<Group>>);

// ---------------------------------------------------------------------------
// Runtime test: for_each_field visitor
// ---------------------------------------------------------------------------

struct FieldCounter {
    int values = 0;
    int belongs_tos = 0;
    int has_manys = 0;
    int many_to_manys = 0;
    int has_ones = 0;
    std::vector<std::string_view> value_names;
    std::vector<std::string_view> bto_names;

    template<class V>
    void value(V&, std::string_view col, dbo::FieldOpts) {
        std::cout << "  value: " << col << " (type: " << std::meta::display_string_of(^^V) << ")\n";
        value_names.push_back(col);
        ++values;
    }

    template<class Target>
    void belongs_to(dbo::ptr<Target>&, dbo::BelongsToOpts bto) {
        std::cout << "  belongs_to<" << std::meta::display_string_of(^^Target)
                  << "> (name: '" << bto.name << "')\n";
        bto_names.push_back(bto.name);
        ++belongs_tos;
    }

    template<class Target>
    void has_many(dbo::collection<dbo::ptr<Target>>&, dbo::HasManyOpts hm) {
        std::cout << "  has_many<" << std::meta::display_string_of(^^Target)
                  << "> (join: '" << hm.join_name << "')\n";
        ++has_manys;
    }

    template<class Target>
    void many_to_many(dbo::junction<Target>&, dbo::HasManyOpts hm) {
        std::cout << "  many_to_many<" << std::meta::display_string_of(^^Target)
                  << "> (join: '" << hm.join_name << "')\n";
        ++many_to_manys;
    }

    template<class Target>
    void has_one(dbo::weak_ptr<Target>&, dbo::HasOneOpts) {
        std::cout << "  has_one<" << std::meta::display_string_of(^^Target) << ">\n";
        ++has_ones;
    }
};

struct AllCounter {
    int& total;
    template<class V> void value(V&, std::string_view, dbo::FieldOpts) { ++total; }
    template<class T> void belongs_to(dbo::ptr<T>&, dbo::BelongsToOpts) { ++total; }
    template<class T> void has_many(dbo::collection<dbo::ptr<T>>&, dbo::HasManyOpts) { ++total; }
    template<class T> void many_to_many(dbo::junction<T>&, dbo::HasManyOpts) { ++total; }
    template<class T> void has_one(dbo::weak_ptr<T>&, dbo::HasOneOpts) { ++total; }
};

int main() {
    std::cout << "=== Wt::Dbo C++26 Reflection Test ===\n\n";

    // Test 1: User member iteration
    {
        std::cout << "User members:\n";
        User u;
        u.name = "Alice";
        u.age = 30;

        FieldCounter counter;
        for_each_field(u, counter);

        std::cout << "\n  values=" << counter.values
                  << " belongs_to=" << counter.belongs_tos
                  << " has_many=" << counter.has_manys
                  << " many_to_many=" << counter.many_to_manys << "\n\n";

        assert(counter.values == 2);       // name, age
        assert(counter.belongs_tos == 1);  // group
        assert(counter.has_manys == 1);    // posts

        // Verify custom column name from dbo_meta<User>
        assert(counter.value_names[0] == "full_name"); // name → "full_name"
        assert(counter.value_names[1] == "age");       // age → "age" (default)

        // Verify custom belongsTo name
        assert(counter.bto_names[0] == "dept");

        std::cout << "  OK: User column names correct\n\n";
    }

    // Test 2: Post member iteration
    {
        std::cout << "Post members:\n";
        Post p;
        p.title = "Hello";
        p.body = "World";

        FieldCounter counter;
        for_each_field(p, counter);

        std::cout << "\n  values=" << counter.values
                  << " belongs_to=" << counter.belongs_tos
                  << " many_to_many=" << counter.many_to_manys << "\n\n";

        assert(counter.values == 2);        // title, body
        assert(counter.belongs_tos == 1);   // author
        assert(counter.many_to_manys == 1); // tags (junction)
    }

    // Test 3: Tag member iteration
    {
        std::cout << "Tag members:\n";
        Tag t;
        t.name = "cpp26";

        FieldCounter counter;
        for_each_field(t, counter);

        std::cout << "\n  values=" << counter.values
                  << " many_to_many=" << counter.many_to_manys << "\n\n";

        assert(counter.values == 1);        // name
        assert(counter.many_to_manys == 1); // posts (junction)
    }

    // Test 4: Verify private members are excluded
    {
        std::cout << "Checking private member exclusion:\n";
        User u;
        int total = 0;

        for_each_field(u, AllCounter{total});

        // User has 5 data members (name, age, group, posts, cache_)
        // but cache_ is private → should be excluded
        // So we expect 4 (name, age, group, posts)
        assert(total == 4);
        std::cout << "  OK: private member 'cache_' excluded (total=" << total << ")\n\n";
    }

    std::cout << "=== All tests passed! ===\n";
    return 0;
}

#else // !USE_CPP26_REFLECTION

int main() {
    std::cout << "C++26 reflection not available. Test skipped.\n";
    return 0;
}

#endif
