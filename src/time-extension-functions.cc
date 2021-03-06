/**
 * Copyright (c) 2015, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file time-extension-functions.cc
 */

#include "config.h"

#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#include <string>
#include <unordered_map>

#include "base/date_time_scanner.hh"
#include "base/lrucache.hpp"
#include "sql_util.hh"
#include "relative_time.hh"

#include "vtab_module.hh"

using namespace std;

static auto_buffer timeslice(sqlite3_value *time_in, nonstd::optional<const char *> slice_in_opt)
{
    thread_local date_time_scanner dts;
    thread_local struct {
        std::string c_slice_str;
        relative_time c_rel_time;
    } cache;
    const auto slice_in = string_fragment(slice_in_opt.value_or("15m"));

    if (slice_in.empty()) {
        throw sqlite_func_error("no time slice value given");
    }

    if (slice_in != cache.c_slice_str.c_str()) {
        auto parse_res = relative_time::from_str(slice_in.data());
        if (parse_res.isErr()) {
            throw sqlite_func_error("unable to parse time slice value: {} -- {}",
                                    slice_in, parse_res.unwrapErr().pe_msg);
        }

        cache.c_rel_time = parse_res.unwrap();
        if (cache.c_rel_time.empty()) {
            throw sqlite_func_error("could not determine a time slice from: {}",
                                    slice_in);
        }

        if (cache.c_rel_time.is_absolute()) {
            throw sqlite_func_error("absolute time slices are not valid");
        }

        cache.c_slice_str = slice_in.to_string();
    }

    int64_t us, remainder;
    struct timeval tv;

    switch (sqlite3_value_type(time_in)) {
        case SQLITE3_TEXT: {
            const char *time_in_str = reinterpret_cast<const char *>(sqlite3_value_text(
                time_in));
            struct exttm tm;

            if (dts.scan(time_in_str, strlen(time_in_str), nullptr, &tm, tv, false) == nullptr) {
                dts.unlock();
                if (dts.scan(time_in_str, strlen(time_in_str), nullptr, &tm, tv, false) == nullptr) {
                    throw sqlite_func_error("unable to parse time value -- {}",
                                            time_in_str);
                }
            }

            us = tv.tv_sec * 1000000LL + tv.tv_usec;
            break;
        }
        case SQLITE_INTEGER: {
            auto msecs = sqlite3_value_int64(time_in);

            us = msecs * 1000LL;
            break;
        }
    }

    remainder = us % cache.c_rel_time.to_microseconds();
    us -= remainder;

    tv.tv_sec = us / (1000LL * 1000LL);
    tv.tv_usec = us % (1000LL * 1000LL);

    auto ts = auto_buffer::alloc(64);
    auto actual_length = sql_strftime(ts.in(), ts.size(), tv);

    ts.shrink_to(actual_length);
    return ts;
}

static
nonstd::optional<double> sql_timediff(const char *time1, const char *time2)
{
    struct timeval tv1, tv2, retval;
    date_time_scanner dts1, dts2;
    auto parse_res1 = relative_time::from_str(time1, -1);

    if (parse_res1.isOk()) {
        tv1 = parse_res1.unwrap().add_now().to_timeval();
    } else if (!dts1.convert_to_timeval(time1, -1, nullptr, tv1)) {
        return nonstd::nullopt;
    }

    auto parse_res2 = relative_time::from_str(time2, -1);
    if (parse_res2.isOk()) {
        tv2 = parse_res2.unwrap().add_now().to_timeval();
    } else if (!dts2.convert_to_timeval(time2, -1, nullptr, tv2)) {
        return nonstd::nullopt;
    }

    timersub(&tv1, &tv2, &retval);

    return (double) retval.tv_sec + (double) retval.tv_usec / 1000000.0;
}

int time_extension_functions(struct FuncDef **basic_funcs,
                             struct FuncDefAgg **agg_funcs)
{
    static struct FuncDef time_funcs[] = {
        sqlite_func_adapter<decltype(&timeslice), timeslice>::builder(
            help_text("timeslice",
                      "Return the start of the slice of time that the given timestamp falls in.")
                .sql_function()
                .with_parameter({"time", "The timestamp to get the time slice for."})
                .with_parameter({"slice", "The size of the time slices"})
                .with_tags({"datetime"})
                .with_example({
                    "To get the timestamp rounded down to the start of the ten minute slice",
                    "SELECT timeslice('2017-01-01T05:05:00', '10m')"
                })
                .with_example({
                    "To group log messages into five minute buckets and count them",
                    "SELECT timeslice(log_time_msecs, '5m') AS slice, count(*) FROM lnav_example_log GROUP BY slice"
                })
        ),

        sqlite_func_adapter<decltype(&sql_timediff), sql_timediff>::builder(
            help_text("timediff",
                      "Compute the difference between two timestamps in seconds")
                .sql_function()
                .with_parameter({"time1", "The first timestamp"})
                .with_parameter({"time2", "The timestamp to subtract from the first"})
                .with_tags({"datetime"})
                .with_example({
                    "To get the difference between two timestamps",
                    "SELECT timediff('2017-02-03T04:05:06', '2017-02-03T04:05:00')"
                })
                .with_example({
                    "To get the difference between relative timestamps",
                    "SELECT timediff('today', 'yesterday')"
                })
        ),

        { nullptr }
    };

    *basic_funcs = time_funcs;
    *agg_funcs   = nullptr;

    return SQLITE_OK;
}
