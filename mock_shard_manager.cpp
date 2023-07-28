#include "mock_homeobject.hpp"
#include "mock_shard_manager.hpp"
#include <functional>
#include <optional>
#include <variant>

namespace homeobject {
static uint64_t get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast< std::chrono::milliseconds >(now.time_since_epoch());
    uint64_t timestamp = static_cast< uint64_t >(duration.count());
    return timestamp;
}

void MockHomeObject::create_shard(pg_id pg_owner, uint64_t size_bytes, ShardManager::id_cb cb) {
    LOGINFO("Creating Shard: in Pg [{}] of Size [{}b]", pg_owner, size_bytes);
    auto err = ShardError::UNKNOWN_PG;
    auto id = shard_id{0};
    {
        auto lg = std::scoped_lock(_pg_lock, _shard_lock);
        if (auto pg_it = _pg_map.find(pg_owner); _pg_map.end() != pg_it) {
            id = _cur_shard_id++;
            pg_it->second.second.emplace(id);
            auto [_, happened] = _shards.emplace(std::make_pair(
                id, ShardInfo{id, pg_owner, ShardState::OPEN, 1024 * 1024 * 1024, get_current_timestamp(), 0, 0, 0}));
            err = happened ? ShardError::OK : ShardError::UNKNOWN;
        }
    }
    if (!cb) return;
    if (err == ShardError::OK) {
        cb(std::variant< shard_id, ShardError >{id}, std::nullopt);
    } else {
        cb(err, std::nullopt);
    }
}

void MockHomeObject::get_shard(shard_id id, ShardManager::info_cb cb) const {
    auto info = ShardInfo();
    auto err = ShardError::UNKNOWN_SHARD;
    {
        auto lg = std::scoped_lock(_pg_lock, _shard_lock);
        if (auto shard_it = _shards.find(id); _shards.end() != shard_it) {
            info = shard_it->second;
            err = ShardError::OK;
        }
    }

    if (!cb) return;
    if (err == ShardError::OK) {
        cb(info, std::nullopt);
    } else {
        cb(err, std::nullopt);
    }
}

void MockHomeObject::list_shards(pg_id id, ShardManager::list_cb cb) const {
    auto info = std::vector< ShardInfo >();
    auto err = ShardError::UNKNOWN_PG;
    {
        auto lg = std::scoped_lock(_pg_lock, _shard_lock);
        if (auto pg_it = _pg_map.find(id); _pg_map.end() != pg_it) {
            err = ShardError::OK;
            for (auto const& shard_id : pg_it->second.second) {
                auto shard_it = _shards.find(shard_id);
                RELEASE_ASSERT(_shards.end() != shard_it, "Missing Shard [{}]!", shard_id);
                info.push_back(shard_it->second);
            }
        }
    }

    if (!cb) return;
    if (err == ShardError::OK) {
        cb(info, std::nullopt);
    } else {
        cb(err, std::nullopt);
    }
}

void MockHomeObject::seal_shard(shard_id id, ShardManager::info_cb cb) {
    auto info = ShardInfo();
    auto err = ShardError::UNKNOWN_SHARD;
    {
        auto lg = std::scoped_lock(_pg_lock, _shard_lock);
        if (auto shard_it = _shards.find(id); _shards.end() != shard_it) {
            shard_it->second.state = ShardState::SEALED;
            info = shard_it->second;
            err = ShardError::OK;
        }
    }

    if (!cb) return;
    if (err == ShardError::OK) {
        cb(info, std::nullopt);
    } else {
        cb(err, std::nullopt);
    }
}

} // namespace homeobject
