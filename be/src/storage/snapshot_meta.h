// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

#pragma once

#include <ostream>
#include <string_view>
#include <vector>

#include "common/statusor.h"
#include "gen_cpp/olap_file.pb.h"
#include "gen_cpp/snapshot.pb.h"
#include "storage/del_vector.h"

namespace starrocks {

class RandomAccessFile;
class WritableFile;

class SnapshotMeta {
public:
    Status serialize_to_file(const std::string& file_path);
    Status serialize_to_file(WritableFile* file);

    Status parse_from_file(RandomAccessFile* file);

    SnapshotTypePB snapshot_type() const { return _snapshot_type; }

    void set_snapshot_type(SnapshotTypePB snapshot_type) { _snapshot_type = snapshot_type; }

    int32_t snapshot_format() const { return _format_version; }

    void set_snapshot_format(int32_t format) { _format_version = format; }

    int64_t snapshot_version() const { return _snapshot_version; }

    void set_snapshot_version(int64_t snapshot_version) { _snapshot_version = snapshot_version; }

    TabletMetaPB& tablet_meta() { return _tablet_meta; }

    const TabletMetaPB& tablet_meta() const { return _tablet_meta; }

    std::vector<RowsetMetaPB>& rowset_metas() { return _rowset_metas; }

    const std::vector<RowsetMetaPB>& rowset_metas() const { return _rowset_metas; }

    std::unordered_map<uint32_t, DelVector>& delete_vectors() { return _delete_vectors; }

    const std::unordered_map<uint32_t, DelVector>& delete_vectors() const { return _delete_vectors; }

private:
    SnapshotTypePB _snapshot_type = SNAPSHOT_TYPE_UNKNOWN;
    int32_t _format_version = -1 /* default invalid value*/;
    int64_t _snapshot_version = -1 /*default invalid value*/;
    TabletMetaPB _tablet_meta;
    std::vector<RowsetMetaPB> _rowset_metas;
    std::unordered_map<uint32_t, DelVector> _delete_vectors;
};

} // namespace starrocks
