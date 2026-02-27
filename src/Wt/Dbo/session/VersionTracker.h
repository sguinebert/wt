// This may look like C code, but it's really -*- C++ -*-
/*
 * Copyright (C) 2026 Sylvain Guinebert, Paris, France.
 *
 * Released under the MIT License.
 *
 * VersionTracker.h — External version tracking for value-oriented DBO.
 *
 * The legacy ptr<T>/MetaDbo<T> system stores version inside MetaDbo.
 * The value-oriented API uses plain structs with no version member,
 * so Session tracks versions externally via this map.
 *
 * Key = (type_index, hash of id)
 * Value = current version number
 *
 * The id is hashed via std::hash<IdType> so any hashable id type
 * (long long, std::string, composite keys with a hash specialization)
 * can be used as a version tracking key.
 */
#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>

namespace Wt {
  namespace Dbo {

struct VersionKey {
    std::type_index type;
    std::size_t id_hash;

    bool operator==(const VersionKey&) const = default;
};

struct VersionKeyHash {
    std::size_t operator()(const VersionKey& k) const noexcept {
        auto h1 = std::hash<std::type_index>{}(k.type);
        return h1 ^ (k.id_hash << 1);
    }
};

class VersionTracker {
public:
    /*! \brief Record or update the version for an object. */
    template<class IdType>
    void set(std::type_index type, const IdType& id, int version) {
        map_[VersionKey{type, std::hash<IdType>{}(id)}] = version;
    }

    /*! \brief Get the version for an object. Returns -1 if not tracked. */
    template<class IdType>
    int get(std::type_index type, const IdType& id) const {
        auto it = map_.find(VersionKey{type, std::hash<IdType>{}(id)});
        return (it != map_.end()) ? it->second : -1;
    }

    /*! \brief Remove tracking for an object. */
    template<class IdType>
    void remove(std::type_index type, const IdType& id) {
        map_.erase(VersionKey{type, std::hash<IdType>{}(id)});
    }

    /*! \brief Clear all tracked versions. */
    void clear() { map_.clear(); }

private:
    std::unordered_map<VersionKey, int, VersionKeyHash> map_;
};

  } // namespace Dbo
} // namespace Wt
