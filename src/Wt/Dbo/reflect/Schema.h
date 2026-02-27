// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2024 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * ReflectSchema.h — C++26 reflection schema mapping.
 *
 * reflect_init_schema<C>(session, mapping) populates ModelInfo::sets
 * and key metadata by iterating over reflected members of C.
 */
#pragma once

#include <string>
#include <tuple>

#include <Wt/Dbo/reflect/Iterators.h>
#include <Wt/Dbo/sql/Util.h>
#include <Wt/Dbo/sql/Traits.h>
#include <Wt/Dbo/reflect/Friend.h>

namespace Wt {
  namespace Dbo {
    namespace Reflect {

namespace detail {

/*! \brief Op functor for schema population (value fields only).
 *
 * Passed to for_each_field_static() to extract natural ID metadata.
 */
struct SchemaBuilder {
    Session& session;
    Impl::ModelInfo& mapping;

    template<class V>
    void value(std::string_view col, FieldOpts opts) {
        if (opts.natural_id) {
            if (!mapping.naturalIdFieldName.empty())
                mapping.naturalIdFieldName += ",";
            mapping.naturalIdFieldName += std::string(col);
            mapping.naturalIdFieldSize = opts.size;
        }
    }

    template<class Target>
    void foreign_key(BelongsToOpts) {
        // FK fields handled by SQL generation — no schema population needed.
    }
};

/*! \brief Populate sets from dbo_meta<C>::relations() tuple. */
template<class C>
void populate_sets_from_relations(Session& session, Impl::ModelInfo& mapping) {
    constexpr auto rels = dbo_meta<C>::relations();

    std::apply([&](const auto&... rel) {
        auto process = [&](const auto& r) {
            using RelType = std::remove_cvref_t<decltype(r)>;
            if constexpr (is_has_many_rel<RelType>::value) {
                using Target = typename RelType::target_type;
                const char* targetTable = session.template tableName<Target>();
                std::string joinName = r.join_name.empty()
                    ? std::string(mapping.tableName)
                    : std::string(r.join_name);

                mapping.sets.push_back(Impl::SetInfo(
                    targetTable,
                    ManyToOne,
                    joinName,
                    std::string(r.fk_field),
                    Impl::FKNotNull | Impl::FKOnDeleteCascade,
                    0));
            }
            else if constexpr (is_many_to_many_rel<RelType>::value) {
                using Target = typename RelType::target_type;
                const char* targetTable = session.template tableName<Target>();
                std::string joinName(r.join_table);

                mapping.sets.push_back(Impl::SetInfo(
                    targetTable,
                    ManyToMany,
                    joinName,
                    std::string(),
                    Impl::FKNotNull | Impl::FKOnDeleteCascade,
                    0));
            }
        };
        (process(rel), ...);
    }, rels);
}

} // namespace detail

// ---------------------------------------------------------------------------
// reflect_init_schema<C> — schema setup from reflection
// ---------------------------------------------------------------------------

/*! \brief Populate ModelInfo for class C using C++26 reflection.
 *
 * Iterates all public/protected members for natural ID detection,
 * then populates relation sets from dbo_meta<C>::relations().
 *
 * Must be called after all referenced classes are already mapClass'd.
 *
 * \ingroup dbo
 */
template<class C>
void reflect_init_schema(Session& session, Impl::ModelInfo& mapping) {
    // Table-level metadata from reflection-driven consteval config.
    constexpr auto meta = dbo_meta<C>::table();

    if constexpr (meta.surrogate_id) {
        if constexpr (!meta.id_field.empty())
            mapping.surrogateIdFieldName = meta.id_field.data();
        else
            mapping.surrogateIdFieldName = "id";
    } else {
        mapping.surrogateIdFieldName = nullptr;
    }

    if constexpr (!meta.version_field.empty())
        mapping.versionFieldName = meta.version_field.data();
    else
        mapping.versionFieldName = nullptr;

    // Extract natural ID info from value fields
    detail::SchemaBuilder builder{session, mapping};
    for_each_field_static<C>(builder);

    // Populate relation sets from dbo_meta<C>::relations()
    detail::populate_sets_from_relations<C>(session, mapping);
}

    } // namespace Reflect
  } // namespace Dbo
} // namespace Wt
