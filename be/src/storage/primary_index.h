// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

#pragma once

#include <string>
#include <unordered_map>

#include "common/status.h"
#include "storage/olap_common.h"
#include "storage/vectorized/chunk_iterator.h"

namespace starrocks {

class Tablet;
class HashIndex;

const uint64_t ROWID_MASK = 0xffffffff;

// An index to lookup a record's position(rowset->segment->rowid) by primary key.
// It's only used to handle updates/deletes in the write pipeline for now.
// Use a simple in-memory hash_map implementation for demo purpose.
class PrimaryIndex {
public:
    using segment_rowid_t = uint32_t;
    using DeletesMap = std::unordered_map<uint32_t, vector<segment_rowid_t>>;
    using tablet_rowid_t = uint64_t;
    using TabletRowidColumn = vectorized::UInt64Column;

    PrimaryIndex();
    PrimaryIndex(const vectorized::Schema& pk_schema);
    ~PrimaryIndex();

    // Fetch all primary keys from the tablet associated with this index into memory
    // to build a hash index.
    //
    // [thread-safe]
    Status load(Tablet* tablet);

    // Reset primary index to unload state, clear all contents
    //
    // [thread-safe]
    void unload();

    // insert new primary keys into this index. caller need to make sure key doesn't exists
    // in index
    // [not thread-safe]
    Status insert(uint32_t rssid, const vector<uint32_t>& rowids, const vectorized::Column& pks);
    Status insert(uint32_t rssid, uint32_t rowid_start, const vectorized::Column& pks);

    // insert new primary keys into this index. if a key already exists in the index, assigns
    // the new record's position to the mapped value corresponding to the key, and save the
    // old position to |deletes|.
    //
    // [not thread-safe]
    void upsert(uint32_t rssid, uint32_t rowid_start, const vectorized::Column& pks, DeletesMap* deletes);

    // used for compaction, try replace input rowsets' rowid with output segment's rowid, if
    // input rowsets' rowid doesn't exist, this indicates that the row of output rowset is
    // deleted during compaction, so append it's rowid into |deletes|
    // |rssid| output segment's rssid
    // |key_col| each output segment row's *encoded* primary key
    // |src_rssid| each output segment row's source segment rssid
    // |failed| rowids of output segment's rows that failed to replace
    //
    // [not thread-safe]
    void try_replace(uint32_t rssid, uint32_t rowid_start, const vectorized::Column& pks,
                     const vector<uint32_t>& src_rssid, vector<uint32_t>* failed);

    // |key_col| contains the *encoded* primary keys to be deleted from this index.
    // The position of deleted keys will be appended into |new_deletes|.
    //
    // [not thread-safe]
    void erase(const vectorized::Column& pks, DeletesMap* deletes);

    void get(const vectorized::Column& pks, std::vector<uint64_t>* rowids) const;

    // [not thread-safe]
    std::size_t memory_usage() const;

    // [not thread-safe]
    std::size_t size() const;

    // [not thread-safe]
    std::size_t capacity() const;

    // [not thread-safe]
    void reserve(size_t s);

    std::string to_string() const;

private:
    void _set_schema(const vectorized::Schema& pk_schema);

    Status _do_load(Tablet* tablet);

    std::mutex _lock;
    std::atomic<bool> _loaded{false};
    Status _status;
    int64_t _tablet_id = 0;
    vectorized::Schema _pk_schema;
    FieldType _enc_pk_type = OLAP_FIELD_TYPE_UNKNOWN;
    std::unique_ptr<HashIndex> _pkey_to_rssid_rowid;
};

inline std::ostream& operator<<(std::ostream& os, const PrimaryIndex& o) {
    os << o.to_string();
    return os;
}

std::unique_ptr<PrimaryIndex> TEST_create_primary_index(const vectorized::Schema& pk_schema);

} // namespace starrocks
