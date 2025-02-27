// This file is made available under Elastic License 2.0.
// This file is based on code available under the Apache license here:
//   https://github.com/apache/incubator-doris/blob/master/be/src/util/timezone_utils.cpp

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
//

#include "util/timezone_utils.h"

namespace starrocks {

RE2 TimezoneUtils::time_zone_offset_format_reg(R"(^[+-]{1}\d{2}\:\d{2}$)");

const std::string TimezoneUtils::default_time_zone = "+08:00";

bool TimezoneUtils::find_cctz_time_zone(const std::string& timezone, cctz::time_zone& ctz) {
    re2::StringPiece value;
    if (time_zone_offset_format_reg.Match(timezone, 0, timezone.size(), RE2::UNANCHORED, &value, 1)) {
        bool positive = (value[0] != '-');

        //Regular expression guarantees hour and minute mush be int
        int hour = std::stoi(value.substr(1, 2).as_string());
        int minute = std::stoi(value.substr(4, 2).as_string());

        // timezone offsets around the world extended from -12:00 to +14:00
        if (!positive && hour > 12) {
            return false;
        } else if (positive && hour > 14) {
            return false;
        }
        int offset = hour * 60 * 60 + minute * 60;
        offset *= positive ? 1 : -1;
        ctz = cctz::fixed_time_zone(cctz::seconds(offset));
        return true;
    } else if (timezone == "CST") {
        // Supports offset and region timezone type, "CST" use here is compatibility purposes.
        ctz = cctz::fixed_time_zone(cctz::seconds(8 * 60 * 60));
        return true;
    } else {
        return cctz::load_time_zone(timezone, &ctz);
    }
}

bool TimezoneUtils::find_cctz_time_zone(const TimezoneHsScan& timezone_hsscan, const std::string& timezone,
                                        cctz::time_zone& ctz) {
    bool v = false;
    hs_scan(
            timezone_hsscan.database, timezone.c_str(), timezone.size(), 0, timezone_hsscan.scratch,
            [](unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void* ctx) -> int {
                *((bool*)ctx) = true;
                return 1;
            },
            &v);

    if (v) {
        bool positive = (timezone.substr(0, 1) != "-");

        //Regular expression guarantees hour and minute mush be int
        int hour = std::stoi(timezone.substr(1, 2));
        int minute = std::stoi(timezone.substr(4, 5));

        // timezone offsets around the world extended from -12:00 to +14:00
        if (!positive && hour > 12) {
            return false;
        } else if (positive && hour > 14) {
            return false;
        }
        int offset = hour * 60 * 60 + minute * 60;
        offset *= positive ? 1 : -1;
        ctz = cctz::fixed_time_zone(cctz::seconds(offset));
        return true;
    }

    if (timezone == "CST") {
        // Supports offset and region timezone type, "CST" use here is compatibility purposes.
        ctz = cctz::fixed_time_zone(cctz::seconds(8 * 60 * 60));
        return true;
    } else {
        return cctz::load_time_zone(timezone, &ctz);
    }
}

int64_t TimezoneUtils::to_utc_offset(const cctz::time_zone& ctz) {
    cctz::time_zone utc = cctz::utc_time_zone();
    const std::chrono::time_point<std::chrono::system_clock> tp;
    const cctz::time_zone::absolute_lookup a = ctz.lookup(tp);
    const cctz::time_zone::absolute_lookup b = utc.lookup(tp);
    return a.cs - b.cs;
}

} // namespace starrocks