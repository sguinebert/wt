# Wt::Dbo Error Model Migration (Lot 2)

Date: 2026-02-27  
Status: Lot 2 mostly complete (core)

## Canonical Types

`Wt::Dbo` error flow is based on:

1. `DboErrc`: stable machine-readable error code enum.
2. `DboError`: structured payload (`code`, `message`, `backend`, `sqlstate`, `vendor_code`, `context`, `table`, `id`, `version`).
3. `dbo_result<T>`: alias for `std::expected<T, dbo_error>`.

## Legacy To Canonical Mapping

1. `StaleObjectException` -> `DboErrc::StaleObject`
2. `ObjectNotFoundException` -> `DboErrc::ObjectNotFound`
3. `NoUniqueResultException` -> `DboErrc::NoUniqueResult`
4. `Exception` (connection failures) -> `DboErrc::Connection`
5. `Exception` (SQL failures) -> `DboErrc::Sql`
6. `Exception` (schema/mapping setup failures) -> `DboErrc::Schema` or `DboErrc::Mapping`
7. Transaction control failures -> `DboErrc::Transaction`
8. JSON/serialization failures -> `DboErrc::Serialization`
9. Build/configuration gate failures -> `DboErrc::Configuration`

## Compatibility Strategy

1. Legacy payload structs (`StaleObjectError`, `SqlError`, etc.) are still accepted as constructors for `dbo_error`.
2. `Compat.h` hard-cut shim keeps `unwrap_or_throw()` for source compatibility, but aborts on error instead of rethrowing legacy exceptions.
3. `Session::with_transaction()` now returns `awaitable<dbo_result<void>>` and expects an `awaitable<dbo_result<void>>` callback.
4. New and migrated code should return `dbo_result<T>` directly and avoid throw/catch-based flow.

## Remaining Exception Boundary

1. In maintained Dbo core, `catch` sites are currently limited to `SqlConnection.h`.
2. These catches are intentional boundary adapters while backend implementations are still exception-based.
