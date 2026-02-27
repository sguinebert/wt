# Wt::Dbo Modern API Showcase (Reflection-First)

Date: 2026-02-27  
Status: Draft for merge

## Scope

This document describes the current modern `Wt::Dbo` API in this repository:

1. C++26 reflection-first mapping.
2. Async + exception-free result flow (`std::expected`).
3. Typed query DSL with member reflections (`^^Type::member`).
4. Value-oriented ORM usage (no `ptr` identity-map style API).

Primary headers:

- `src/Wt/Dbo/session/Session.h`
- `src/Wt/Dbo/session/Query.h`
- `src/Wt/Dbo/reflect/Meta.h`
- `src/Wt/Dbo/reflect/Registry.h`
- `src/Wt/Dbo/core/Error.h`
- `src/Wt/Dbo/reflect/Json.h`

## Core Principles

1. Single metadata source of truth: `dbo_meta<T>`.
2. Member-driven API surface: select/order/group/filter on reflected members.
3. Runtime does execution, compile time does structure.
4. Public flow is `dbo_result<T>` and `awaitable<dbo_result<T>>`.
5. User keeps full freedom to write raw SQL when needed.

## Quick Setup

```cpp
#include <Wt/Dbo/session/Session.h>
#include <Wt/Dbo/session/Query.h>
#include <Wt/Dbo/reflect/Meta.h>
#include <Wt/Dbo/reflect/Registry.h>
#include <Wt/Dbo/core/fk.h>
```

## 1. Model Declaration (Plain Struct + Metadata)

```cpp
struct DboModelTag {};

struct User : DboModelTag {
  long long id{};
  std::string email;
  int age{};
};

struct Post : DboModelTag {
  long long id{};
  std::string title;
  Wt::Dbo::fk<User> author;
  bool published{};
};

struct Tag : DboModelTag {
  long long id{};
  std::string name;
};

template<>
struct Wt::Dbo::dbo_meta<User> {
  static consteval TableOpts table() {
    return {.table_name = "users", .id_field = "id", .surrogate_id = true};
  }

  static consteval auto relations() {
    return std::tuple{
      has_many_rel<Post>{}
    };
  }
};

template<>
struct Wt::Dbo::dbo_meta<Post> {
  static consteval TableOpts table() {
    return {.table_name = "posts", .id_field = "id", .surrogate_id = true};
  }

  static consteval auto relations() {
    return std::tuple{
      many_to_many_rel<Tag>{.join_table = "post_tags"}
    };
  }
};
```

## 2. Compile-Time Model Discovery (Consteval Registry)

Use tag-based program discovery:

```cpp
WT_DBO_DECLARE_CONSTEVAL_SESSION_MODELS_AUTO_FROM_TAG(DboModelTag);
```

This wires:

- `Reflect::ConstevalRegistry<Wt::Dbo::Session>::models`

No runtime `mapClass()` registration path is required in the current model.

## 3. Zero-Literal Typed Select (Auto Result Type)

```cpp
Wt::Dbo::Session session;

// Result type auto-deduced: std::tuple<long long, std::string>
auto q = session.select<^^User::id, ^^User::email>();
auto rows = co_await q.resultList(); // awaitable<dbo_result<std::vector<std::tuple<...>>>>
```

Single selected member auto-deduces to scalar result:

```cpp
auto q1 = session.select<^^User::id>();
auto ids = co_await q1.resultList(); // std::vector<long long>
```

## 4. Typed Predicate DSL

Member references:

```cpp
using Wt::Dbo::col;
```

Comparison and logical composition:

```cpp
auto q = session.select<^^User::id, ^^User::email>()
  .where((col<^^User::age> >= 18) && (col<^^User::email> != std::string{}))
  .orWhere(!Wt::Dbo::isNull<^^User::email>());
```

Supported predicate helpers:

1. `eq/neq/lt/lte/gt/gte`
2. `in/notIn`
3. `isNull/isNotNull`
4. `between`
5. `like/notLike`
6. `&&`, `||`, `!` composition

Example:

```cpp
auto q = session.select<^^Post::id, ^^Post::title>()
  .where(
    Wt::Dbo::in<^^Post::id>(1LL, 2LL, 3LL) &&
    Wt::Dbo::between<^^Post::id>(1LL, 1000LL) &&
    Wt::Dbo::like<^^Post::title>(std::string{"C++%"})
  );
```

## 5. Typed Order / Group / Having

```cpp
auto q = session.select<^^User::email>()
  .groupBy<^^User::email>()
  .having(Wt::Dbo::gt<^^User::age>(21))
  .orderBy<^^User::email>(false);
```

Multi-member helpers:

```cpp
auto q2 = session.select<^^User::id, ^^User::email>()
  .groupByMembers<^^User::id, ^^User::email>()
  .orderByMembers<
    Wt::Dbo::By<^^User::email>,
    Wt::Dbo::DescBy<^^User::id>
  >();
```

## 6. Aggregate API on Reflected Members

```cpp
auto c = co_await session.count<^^User::id>().resultValue();
auto s = co_await session.sum<^^User::age>().resultValue();
auto a = co_await session.avg<^^User::age>().resultValue();
auto m = co_await session.max<^^User::age>().resultValue();
auto d = co_await session.countDistinct<^^User::email>().resultValue();
```

## 7. Value-Oriented CRUD (No ptr API)

Insert:

```cpp
User u{.email = "sylvain@example.com", .age = 34};
auto created = co_await session.insertValue(std::move(u)); // dbo_result<User>
```

Get / update / remove:

```cpp
auto loaded = co_await session.getValue<User>(123);
if (!loaded) co_return;

User copy = loaded.value();
copy.age = 35;
auto up = co_await session.updateValue(copy);   // dbo_result<void>
auto rm = co_await session.removeValue<User>(123);
```

Bulk:

```cpp
std::vector<User> batch{/* ... */};
auto inserted = co_await session.insertMany(std::move(batch));
auto updated = co_await session.updateMany(inserted.value());
auto removed = co_await session.removeMany<User>(std::vector<long long>{1,2,3});
```

## 8. Relation Access API

One-to-many style fetch:

```cpp
auto posts = co_await session.related<User, Post>(123);
```

Many-to-many fetch:

```cpp
auto tags = co_await session.relatedM2M<Post, Tag>(456);
```

`relatedM2M` uses `dbo_meta<Owner>::relations()` descriptors (`many_to_many_rel<Target>`).

## 9. Typed Join Helpers + Raw SQL Escape Hatch

Typed member-based joins:

```cpp
auto q = session.query<Post>("select p.* from \"posts\" p")
  .join<^^Post::author>("p", "u")
  .where("u.email like ?").bind(std::string{"%@example.com"});
```

Raw SQL remains first-class:

```cpp
auto q = session.query<std::tuple<long long, std::string>>(
  "select u.id, u.email from \"users\" u where u.age > ?"
).bind(18);
```

This hybrid model is intentional:

1. Typed reflection path for correctness and boilerplate elimination.
2. Full literal SQL path for advanced or vendor-specific statements.

## 10. Transactions with `std::expected`

```cpp
auto txResult = co_await session.with_transaction([&]() -> Wt::Dbo::awaitable<Wt::Dbo::dbo_result<void>> {
  auto created = co_await session.insertValue(User{.email = "a@b.c", .age = 20});
  if (!created) {
    co_return std::unexpected(created.error());
  }
  co_return Wt::Dbo::dbo_result<void>{};
});
```

Contract:

1. Callback returns `awaitable<dbo_result<void>>`.
2. On error, rollback is triggered.
3. On success, commit is triggered.

## 11. Exception-Free Error Flow

Canonical types:

1. `DboErrc`
2. `DboError` / `dbo_error`
3. `dbo_result<T> = std::expected<T, dbo_error>`

Pattern:

```cpp
auto r = co_await session.getValue<User>(1);
if (!r) {
  const auto& e = r.error();
  // inspect e.code, e.message, e.table, e.sqlstate, ...
  co_return;
}
```

## 12. Reflection JSON Utilities

```cpp
User u{.id = 1, .email = "x@y.z", .age = 42};
std::string s = Wt::Dbo::Reflect::dbo_to_json(u);

User decoded{};
bool ok = Wt::Dbo::Reflect::dbo_from_json(decoded, s);
```

Behavior:

1. Value members serialized with effective column names.
2. `fk<T>` serialized as FK id.
3. Uses reflection traversal (`for_each_field`) + Glaze.

## 13. Schema Lifecycle

```cpp
auto create = co_await session.createTables();
auto ddl = co_await session.tableCreationSql();
auto drop = co_await session.dropTables();
```

The same metadata drives:

1. Field extraction.
2. Core SQL templates.
3. DDL and relation DDL.

## 14. Current Compile-Time Guarantees

The typed APIs enforce compile-time constraints (via concepts/static_assert), including:

1. Selected members must belong to registered models.
2. Aggregate members must be valid for requested operation (e.g. numeric for `sum`/`avg`).
3. Auto-join path discovery must find relation connectivity between owners.

Current auto-join assumptions in `select<>` implementation:

1. Direct FK edge per join step in discovered path.
2. Joined models use `surrogate_id = true`.

## 15. What Is Uniquely Strong Here

Compared to conventional ORMs, this API combines in one coherent model:

1. Reflection-native typed DSL over real members (`^^Type::member`).
2. Auto-deduced result shape for multi-member `select`.
3. Compile-time model discovery (`ConstevalRegistry`) instead of runtime registry wiring.
4. Async `awaitable` + `std::expected` everywhere in the public flow.
5. Value-first CRUD and FK wrappers (`fk<T>`) with no hidden identity-map semantics.
6. Raw SQL escape hatch without sacrificing typed high-level APIs.

This gives both:

1. Strong compile-time safety and low boilerplate.
2. Low-friction freedom for advanced SQL use cases.
