/**
 * Copyright (c) 2013, Timothy Stack
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
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file session_data.cc
 */

#include <algorithm>
#include <utility>

#include "session_data.hh"

#include <glob.h>
#include <stdio.h>
#include <sys/types.h>
#include <yajl/api/yajl_tree.h>

#include "base/fs_util.hh"
#include "base/isc.hh"
#include "base/opt_util.hh"
#include "base/paths.hh"
#include "bookmarks.json.hh"
#include "bound_tags.hh"
#include "command_executor.hh"
#include "config.h"
#include "hasher.hh"
#include "lnav.events.hh"
#include "lnav.hh"
#include "lnav.indexing.hh"
#include "log_format_ext.hh"
#include "logfile.hh"
#include "service_tags.hh"
#include "sql_util.hh"
#include "sqlitepp.client.hh"
#include "tailer/tailer.looper.hh"
#include "vtab_module.hh"
#include "yajlpp/yajlpp.hh"
#include "yajlpp/yajlpp_def.hh"

session_data_t session_data;
recent_refs_t recent_refs;

static const char* const LOG_METADATA_NAME = "log_metadata.db";

static const char* const META_TABLE_DEF = R"(
CREATE TABLE IF NOT EXISTS bookmarks (
    log_time datetime,
    log_format varchar(64),
    log_hash varchar(128),
    session_time integer,
    part_name text,
    access_time datetime DEFAULT CURRENT_TIMESTAMP,
    comment text DEFAULT '',
    tags text DEFAULT '',
    annotations text DEFAULT NULL,
    log_opid text DEFAULT NULL,

    PRIMARY KEY (log_time, log_format, log_hash, session_time)
);

CREATE TABLE IF NOT EXISTS time_offset (
    log_time datetime,
    log_format varchar(64),
    log_hash varchar(128),
    session_time integer,
    offset_sec integer,
    offset_usec integer,
    access_time datetime DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (log_time, log_format, log_hash, session_time)
);

CREATE TABLE IF NOT EXISTS recent_netlocs (
    netloc text,

    access_time datetime DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (netloc)
);

CREATE TABLE IF NOT EXISTS regex101_entries (
    format_name text NOT NULL,
    regex_name text NOT NULL,
    permalink text NOT NULL,
    delete_code text NOT NULL,

    PRIMARY KEY (format_name, regex_name),

    CHECK(
       format_name  <> '' AND
       regex_name   <> '' AND
       permalink    <> '')
);
)";

static const char* const BOOKMARK_LRU_STMT
    = "DELETE FROM bookmarks WHERE access_time <= "
      "  (SELECT access_time FROM bookmarks "
      "   ORDER BY access_time DESC LIMIT 1 OFFSET 50000)";

static const char* const NETLOC_LRU_STMT
    = "DELETE FROM recent_netlocs WHERE access_time <= "
      "  (SELECT access_time FROM bookmarks "
      "   ORDER BY access_time DESC LIMIT 1 OFFSET 10)";

static const char* const UPGRADE_STMTS[] = {
    R"(ALTER TABLE bookmarks ADD COLUMN comment text DEFAULT '';)",
    R"(ALTER TABLE bookmarks ADD COLUMN tags text DEFAULT '';)",
    R"(ALTER TABLE bookmarks ADD COLUMN annotations text DEFAULT NULL;)",
    R"(ALTER TABLE bookmarks ADD COLUMN log_opid text DEFAULT NULL;)",
};

static constexpr size_t MAX_SESSIONS = 8;
static constexpr size_t MAX_SESSION_FILE_COUNT = 256;

struct session_line {
    session_line(struct timeval tv,
                 intern_string_t format_name,
                 std::string line_hash)
        : sl_time(tv), sl_format_name(format_name),
          sl_line_hash(std::move(line_hash))
    {
    }

    struct timeval sl_time;
    intern_string_t sl_format_name;
    std::string sl_line_hash;
};

static std::vector<session_line> marked_session_lines;
static std::vector<session_line> offset_session_lines;

static bool
bind_line(sqlite3* db,
          sqlite3_stmt* stmt,
          content_line_t cl,
          time_t session_time)
{
    auto& lss = lnav_data.ld_log_source;
    auto lf = lss.find(cl);

    if (lf == nullptr) {
        return false;
    }

    sqlite3_clear_bindings(stmt);

    auto line_iter = lf->begin() + cl;
    auto fr = lf->get_file_range(line_iter, false);
    auto read_result = lf->read_range(fr);

    if (read_result.isErr()) {
        return false;
    }

    auto line_hash = read_result
                         .map([cl](auto sbr) {
                             return hasher()
                                 .update(sbr.get_data(), sbr.length())
                                 .update(cl)
                                 .to_string();
                         })
                         .unwrap();

    return bind_values(stmt,
                       lf->original_line_time(line_iter),
                       lf->get_format()->get_name(),
                       line_hash,
                       session_time)
        == SQLITE_OK;
}

struct session_file_info {
    session_file_info(int timestamp, std::string id, std::string path)
        : sfi_timestamp(timestamp), sfi_id(std::move(id)),
          sfi_path(std::move(path))
    {
    }

    bool operator<(const session_file_info& other) const
    {
        if (this->sfi_timestamp < other.sfi_timestamp) {
            return true;
        }
        if (this->sfi_path < other.sfi_path) {
            return true;
        }
        return false;
    }

    int sfi_timestamp;
    std::string sfi_id;
    std::string sfi_path;
};

static void
cleanup_session_data()
{
    static_root_mem<glob_t, globfree> session_file_list;
    std::list<struct session_file_info> session_info_list;
    std::map<std::string, int> session_count;
    auto session_file_pattern = lnav::paths::dotlnav() / "*-*.ts*.json";

    if (glob(
            session_file_pattern.c_str(), 0, nullptr, session_file_list.inout())
        == 0)
    {
        for (size_t lpc = 0; lpc < session_file_list->gl_pathc; lpc++) {
            const char* path = session_file_list->gl_pathv[lpc];
            char hash_id[64];
            int timestamp;
            const char* base;

            base = strrchr(path, '/');
            if (base == nullptr) {
                continue;
            }
            base += 1;
            if (sscanf(base, "file-%63[^.].ts%d.json", hash_id, &timestamp)
                == 2)
            {
                session_count[hash_id] += 1;
                session_info_list.emplace_back(timestamp, hash_id, path);
            }
            if (sscanf(base,
                       "view-info-%63[^.].ts%d.ppid%*d.json",
                       hash_id,
                       &timestamp)
                == 2)
            {
                session_count[hash_id] += 1;
                session_info_list.emplace_back(timestamp, hash_id, path);
            }
        }
    }

    session_info_list.sort();

    size_t session_loops = 0;

    while (session_info_list.size() > MAX_SESSION_FILE_COUNT) {
        const session_file_info& front = session_info_list.front();

        session_loops += 1;
        if (session_loops < MAX_SESSION_FILE_COUNT
            && session_count[front.sfi_id] == 1)
        {
            session_info_list.splice(session_info_list.end(),
                                     session_info_list,
                                     session_info_list.begin());
        } else {
            if (remove(front.sfi_path.c_str()) != 0) {
                log_error("Unable to remove session file: %s -- %s",
                          front.sfi_path.c_str(),
                          strerror(errno));
            }
            session_count[front.sfi_id] -= 1;
            session_info_list.pop_front();
        }
    }

    session_info_list.sort();

    while (session_info_list.size() > MAX_SESSION_FILE_COUNT) {
        const session_file_info& front = session_info_list.front();

        if (remove(front.sfi_path.c_str()) != 0) {
            log_error("Unable to remove session file: %s -- %s",
                      front.sfi_path.c_str(),
                      strerror(errno));
        }
        session_count[front.sfi_id] -= 1;
        session_info_list.pop_front();
    }
}

void
init_session()
{
    lnav_data.ld_session_time = time(nullptr);
    lnav_data.ld_session_id.clear();
    session_data.sd_view_states[LNV_LOG].vs_top = -1;
}

static std::optional<std::string>
compute_session_id()
{
    bool has_files = false;
    hasher h;

    for (auto& ld_file_name : lnav_data.ld_active_files.fc_file_names) {
        if (!ld_file_name.second.loo_include_in_session) {
            continue;
        }
        has_files = true;
        h.update(ld_file_name.first);
    }
    for (const auto& lf : lnav_data.ld_active_files.fc_files) {
        if (lf->is_valid_filename()) {
            continue;
        }
        if (!lf->get_open_options().loo_include_in_session) {
            continue;
        }

        has_files = true;
        h.update(lf->get_content_id());
    }
    if (!has_files) {
        return std::nullopt;
    }

    return h.to_string();
}

std::optional<session_pair_t>
scan_sessions()
{
    static_root_mem<glob_t, globfree> view_info_list;

    cleanup_session_data();

    const auto session_id = compute_session_id();
    if (!session_id) {
        return std::nullopt;
    }
    auto& session_file_names = lnav_data.ld_session_id[session_id.value()];
    session_file_names.clear();

    auto view_info_pattern_base
        = fmt::format(FMT_STRING("view-info-{}.*.json"), session_id.value());
    auto view_info_pattern = lnav::paths::dotlnav() / view_info_pattern_base;
    if (glob(view_info_pattern.c_str(), 0, nullptr, view_info_list.inout())
        == 0)
    {
        for (size_t lpc = 0; lpc < view_info_list->gl_pathc; lpc++) {
            const char* path = view_info_list->gl_pathv[lpc];
            int timestamp, ppid, rc;
            const char* base;

            base = strrchr(path, '/');
            if (base == nullptr) {
                continue;
            }
            base += 1;
            if ((rc = sscanf(base,
                             "view-info-%*[^.].ts%d.ppid%d.json",
                             &timestamp,
                             &ppid))
                == 2)
            {
                ppid_time_pair_t ptp;

                ptp.first = (ppid == getppid()) ? 1 : 0;
                ptp.second = timestamp;
                session_file_names.emplace_back(ptp, path);
            }
        }
    }

    session_file_names.sort();

    while (session_file_names.size() > MAX_SESSIONS) {
        const std::string& name = session_file_names.front().second;

        if (remove(name.c_str()) != 0) {
            log_error("Unable to remove session: %s -- %s",
                      name.c_str(),
                      strerror(errno));
        }
        session_file_names.pop_front();
    }

    if (session_file_names.empty()) {
        return std::nullopt;
    }

    return std::make_optional(session_file_names.back());
}

void
load_time_bookmarks()
{
    static const char* const BOOKMARK_STMT = R"(
       SELECT
         log_time,
         log_format,
         log_hash,
         session_time,
         part_name,
         access_time,
         comment,
         tags,
         annotations,
         log_opid,
         session_time=? AS same_session
       FROM bookmarks WHERE
         log_time BETWEEN ? AND ? AND
         log_format = ?
       ORDER BY same_session DESC, session_time DESC
)";

    static const char* const TIME_OFFSET_STMT = R"(
       SELECT
         *,
         session_time=? AS same_session
       FROM time_offset
       WHERE
         log_hash = ? AND
         log_time BETWEEN ? AND ? AND
         log_format = ?
       ORDER BY
         same_session DESC,
         session_time DESC
)";

    static auto op = lnav_operation{__FUNCTION__};

    auto op_guard = lnav_opid_guard::internal(op);
    auto& lss = lnav_data.ld_log_source;
    auto_sqlite3 db;
    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_mem<sqlite3_stmt> stmt(sqlite3_finalize);
    bool reload_needed = false;
    auto_mem<char, sqlite3_free> errmsg;

    log_info("loading bookmark db: %s", db_path.c_str());

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        return;
    }

    for (const char* upgrade_stmt : UPGRADE_STMTS) {
        auto rc = sqlite3_exec(
            db.in(), upgrade_stmt, nullptr, nullptr, errmsg.out());
        if (rc != SQLITE_OK) {
            auto exterr = sqlite3_extended_errcode(db.in());
            log_error("unable to upgrade bookmark table -- (%d/%d) %s",
                      rc,
                      exterr,
                      errmsg.in());
        }
    }

    {
        auto netloc_prep_res
            = prepare_stmt(db.in(), "SELECT netloc FROM recent_netlocs");
        if (netloc_prep_res.isErr()) {
            log_error("unable to get netlocs: %s",
                      netloc_prep_res.unwrapErr().c_str());
            return;
        }

        auto netloc_stmt = netloc_prep_res.unwrap();
        bool done = false;

        while (!done) {
            done = netloc_stmt.fetch_row<std::string>().match(
                [](const std::string& netloc) {
                    recent_refs.rr_netlocs.insert(netloc);
                    return false;
                },
                [](const prepared_stmt::fetch_error& fe) {
                    log_error("failed to fetch netloc row: %s",
                              fe.fe_msg.c_str());
                    return true;
                },
                [](prepared_stmt::end_of_rows) { return true; });
        }
    }

    log_info("BEGIN select bookmarks");
    if (sqlite3_prepare_v2(db.in(), BOOKMARK_STMT, -1, stmt.out(), nullptr)
        != SQLITE_OK)
    {
        log_error("could not prepare bookmark select statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    for (auto file_iter = lnav_data.ld_log_source.begin();
         file_iter != lnav_data.ld_log_source.end();
         ++file_iter)
    {
        auto lf = (*file_iter)->get_file();
        if (lf == nullptr) {
            continue;
        }
        if (lf->size() == 0) {
            continue;
        }
        const auto* format = lf->get_format_ptr();
        content_line_t base_content_line;

        base_content_line = lss.get_file_base_content_line(file_iter);

        auto low_line_iter = lf->begin();
        auto high_line_iter = lf->end();

        --high_line_iter;

        if (bind_values(stmt.in(),
                        lnav_data.ld_session_load_time,
                        lf->original_line_time(low_line_iter),
                        lf->original_line_time(high_line_iter),
                        lf->get_format()->get_name())
            != SQLITE_OK)
        {
            return;
        }

        date_time_scanner dts;
        bool done = false;
        int64_t last_mark_time = -1;

        while (!done) {
            int rc = sqlite3_step(stmt.in());

            switch (rc) {
                case SQLITE_OK:
                case SQLITE_DONE:
                    done = true;
                    break;

                case SQLITE_ROW: {
                    const char* log_time
                        = (const char*) sqlite3_column_text(stmt.in(), 0);
                    const char* log_hash
                        = (const char*) sqlite3_column_text(stmt.in(), 2);
                    int64_t mark_time = sqlite3_column_int64(stmt.in(), 3);
                    const char* part_name
                        = (const char*) sqlite3_column_text(stmt.in(), 4);
                    const char* comment
                        = (const char*) sqlite3_column_text(stmt.in(), 6);
                    const char* tags
                        = (const char*) sqlite3_column_text(stmt.in(), 7);
                    const auto annotations = sqlite3_column_text(stmt.in(), 8);
                    const auto log_opid = sqlite3_column_text(stmt.in(), 9);
                    timeval log_tv;
                    exttm log_tm;

                    if (last_mark_time == -1) {
                        last_mark_time = mark_time;
                    } else if (last_mark_time != mark_time) {
                        done = true;
                        continue;
                    }

                    if (part_name == nullptr) {
                        continue;
                    }

                    if (dts.scan(log_time,
                                 strlen(log_time),
                                 nullptr,
                                 &log_tm,
                                 log_tv)
                        == nullptr)
                    {
                        log_warning("bad log time: %s", log_time);
                        continue;
                    }

                    auto line_iter = format->lf_time_ordered
                        ? std::lower_bound(lf->begin(), lf->end(), log_tv)
                        : lf->begin();
                    while (line_iter != lf->end()) {
                        const auto line_tv = line_iter->get_timeval();

                        // NB: only milliseconds were stored in the DB, but the
                        // internal rep stores micros now.
                        if (line_tv.tv_sec != log_tv.tv_sec
                            || line_tv.tv_usec / 1000 != log_tv.tv_usec / 1000)
                        {
                            if (format->lf_time_ordered) {
                                break;
                            }
                            ++line_iter;
                            continue;
                        }

                        auto cl = content_line_t(
                            std::distance(lf->begin(), line_iter));
                        auto fr = lf->get_file_range(line_iter, false);
                        auto read_result = lf->read_range(fr);

                        if (read_result.isErr()) {
                            break;
                        }

                        auto sbr = read_result.unwrap();

                        auto line_hash
                            = hasher()
                                  .update(sbr.get_data(), sbr.length())
                                  .update(cl)
                                  .to_string();

                        if (line_hash != log_hash) {
                            // Using the formatted line for JSON-lines logs was
                            // a mistake in earlier versions. To carry forward
                            // older bookmarks, we need to replicate the bad
                            // behavior.
                            auto hack_read_res
                                = lf->read_line(line_iter, {false, true});
                            if (hack_read_res.isErr()) {
                                break;
                            }
                            auto hack_sbr = hack_read_res.unwrap();
                            auto hack_hash = hasher()
                                                 .update(hack_sbr.get_data(),
                                                         hack_sbr.length())
                                                 .update(cl)
                                                 .to_string();
                            if (hack_hash == log_hash) {
                                log_trace("needed hack to match line: %s:%d",
                                          lf->get_filename_as_string().c_str(),
                                          cl);
                            } else {
                                ++line_iter;
                                continue;
                            }
                        }
                        auto& bm_meta = lf->get_bookmark_metadata();
                        auto line_number = static_cast<uint32_t>(
                            std::distance(lf->begin(), line_iter));
                        content_line_t line_cl
                            = content_line_t(base_content_line + line_number);
                        bool meta = false;

                        if (part_name != nullptr && part_name[0] != '\0') {
                            lss.set_user_mark(&textview_curses::BM_PARTITION,
                                              line_cl);
                            bm_meta[line_number].bm_name = part_name;
                            meta = true;
                        }
                        if (comment != nullptr && comment[0] != '\0') {
                            lss.set_user_mark(&textview_curses::BM_META,
                                              line_cl);
                            bm_meta[line_number].bm_comment = comment;
                            meta = true;
                        }
                        if (tags != nullptr && tags[0] != '\0') {
                            auto_mem<yajl_val_s> tag_list(yajl_tree_free);
                            char error_buffer[1024];

                            tag_list = yajl_tree_parse(
                                tags, error_buffer, sizeof(error_buffer));
                            if (!YAJL_IS_ARRAY(tag_list.in())) {
                                log_error("invalid tags column: %s", tags);
                            } else {
                                lss.set_user_mark(&textview_curses::BM_META,
                                                  line_cl);
                                for (size_t lpc = 0;
                                     lpc < tag_list.in()->u.array.len;
                                     lpc++)
                                {
                                    yajl_val elem
                                        = tag_list.in()->u.array.values[lpc];

                                    if (!YAJL_IS_STRING(elem)) {
                                        continue;
                                    }
                                    bookmark_metadata::KNOWN_TAGS.insert(
                                        elem->u.string);
                                    bm_meta[line_number].add_tag(
                                        elem->u.string);
                                }
                            }
                            meta = true;
                        }
                        if (annotations != nullptr && annotations[0] != '\0') {
                            static const intern_string_t SRC
                                = intern_string::lookup("annotations");

                            const auto anno_sf
                                = string_fragment::from_c_str(annotations);
                            auto parse_res
                                = logmsg_annotations_handlers.parser_for(SRC)
                                      .of(anno_sf);
                            if (parse_res.isErr()) {
                                log_error(
                                    "unable to parse annotations JSON -- "
                                    "%s",
                                    parse_res.unwrapErr()[0]
                                        .to_attr_line()
                                        .get_string()
                                        .c_str());
                            } else if (bm_meta.find(line_number)
                                       == bm_meta.end())
                            {
                                lss.set_user_mark(&textview_curses::BM_META,
                                                  line_cl);
                                bm_meta[line_number].bm_annotations
                                    = parse_res.unwrap();
                                meta = true;
                            } else {
                                meta = true;
                            }
                        }
                        if (log_opid != nullptr && log_opid[0] != '\0') {
                            auto opid_sf
                                = string_fragment::from_c_str(log_opid);
                            lf->set_logline_opid(line_number, opid_sf);
                            meta = true;
                        }
                        if (!meta) {
                            marked_session_lines.emplace_back(
                                lf->original_line_time(line_iter),
                                format->get_name(),
                                line_hash);
                            lss.set_user_mark(&textview_curses::BM_USER,
                                              line_cl);
                        }
                        reload_needed = true;
                        break;
                    }
                    break;
                }

                default: {
                    const char* errmsg;

                    errmsg = sqlite3_errmsg(lnav_data.ld_db);
                    log_error(
                        "bookmark select error: code %d -- %s", rc, errmsg);
                    done = true;
                } break;
            }
        }

        sqlite3_reset(stmt.in());
    }
    log_info("END select bookmarks");

    log_info("BEGIN select time_offset");
    if (sqlite3_prepare_v2(db.in(), TIME_OFFSET_STMT, -1, stmt.out(), nullptr)
        != SQLITE_OK)
    {
        log_error("could not prepare time_offset select statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    for (auto file_iter = lnav_data.ld_log_source.begin();
         file_iter != lnav_data.ld_log_source.end();
         ++file_iter)
    {
        auto lf = (*file_iter)->get_file();
        content_line_t base_content_line;

        if (lf == nullptr) {
            continue;
        }
        if (lf->size() == 0) {
            continue;
        }

        lss.find(lf->get_filename_as_string().c_str(), base_content_line);

        auto low_line_iter = lf->begin();
        auto high_line_iter = lf->end();

        --high_line_iter;

        if (bind_values(stmt.in(),
                        lnav_data.ld_session_load_time,
                        lf->get_content_id(),
                        lf->original_line_time(low_line_iter),
                        lf->original_line_time(high_line_iter),
                        lf->get_format()->get_name())
            != SQLITE_OK)
        {
            return;
        }

        date_time_scanner dts;
        auto done = false;
        int64_t last_mark_time = -1;

        while (!done) {
            const auto rc = sqlite3_step(stmt.in());

            switch (rc) {
                case SQLITE_OK:
                case SQLITE_DONE:
                    done = true;
                    break;

                case SQLITE_ROW: {
                    const auto* log_time
                        = (const char*) sqlite3_column_text(stmt.in(), 0);
                    int64_t mark_time = sqlite3_column_int64(stmt.in(), 3);
                    timeval log_tv;
                    exttm log_tm;

                    if (last_mark_time == -1) {
                        last_mark_time = mark_time;
                    } else if (last_mark_time != mark_time) {
                        done = true;
                        continue;
                    }

                    if (sqlite3_column_type(stmt.in(), 4) == SQLITE_NULL) {
                        continue;
                    }

                    if (!dts.scan(log_time,
                                  strlen(log_time),
                                  nullptr,
                                  &log_tm,
                                  log_tv))
                    {
                        continue;
                    }

                    auto line_iter
                        = lower_bound(lf->begin(), lf->end(), log_tv);
                    while (line_iter != lf->end()) {
                        auto line_tv = line_iter->get_timeval();

                        if ((line_tv.tv_sec != log_tv.tv_sec)
                            || (line_tv.tv_usec / 1000
                                != log_tv.tv_usec / 1000))
                        {
                            break;
                        }

                        int file_line = std::distance(lf->begin(), line_iter);
                        timeval offset;

                        offset_session_lines.emplace_back(
                            lf->original_line_time(line_iter),
                            lf->get_format_ptr()->get_name(),
                            lf->get_content_id());
                        offset.tv_sec = sqlite3_column_int64(stmt.in(), 4);
                        offset.tv_usec = sqlite3_column_int64(stmt.in(), 5);
                        lf->adjust_content_time(file_line, offset);

                        reload_needed = true;

                        ++line_iter;
                    }
                    break;
                }

                default: {
                    const auto* errmsg = sqlite3_errmsg(lnav_data.ld_db);
                    log_error(
                        "bookmark select error: code %d -- %s", rc, errmsg);
                    done = true;
                    break;
                }
            }
        }

        sqlite3_reset(stmt.in());
    }
    log_info("END select time_offset");

    if (reload_needed) {
        lnav_data.ld_views[LNV_LOG].reload_data();
    }
}

static int
read_files(yajlpp_parse_context* ypc,
           const unsigned char* str,
           size_t len,
           yajl_string_props_t*)
{
    return 1;
}

static const json_path_container view_def_handlers = {
    json_path_handler("top_line").for_field(&view_state::vs_top),
    json_path_handler("focused_line").for_field(&view_state::vs_selection),
    json_path_handler("anchor").for_field(&view_state::vs_anchor),
    json_path_handler("search").for_field(&view_state::vs_search),
    json_path_handler("word_wrap").for_field(&view_state::vs_word_wrap),
    json_path_handler("filtering").for_field(&view_state::vs_filtering),
    json_path_handler("commands#").for_field(&view_state::vs_commands),
};

static const struct json_path_container view_handlers = {
    yajlpp::pattern_property_handler("(?<view_name>[\\w\\-]+)")
        .with_obj_provider<view_state, session_data_t>(
            +[](const yajlpp_provider_context& ypc, session_data_t* root) {
                auto view_index_opt
                    = view_from_string(ypc.get_substr("view_name").c_str());
                if (view_index_opt) {
                    return &root->sd_view_states[view_index_opt.value()];
                }

                log_error("unknown view name: %s",
                          ypc.get_substr("view_name").c_str());
                static view_state dummy;
                return &dummy;
            })
        .with_children(view_def_handlers),
};

static const struct json_path_container file_state_handlers = {
    yajlpp::property_handler("visible")
        .with_description("Indicates whether the file is visible or not")
        .for_field(&file_state::fs_is_visible),
};

static const struct json_path_container file_states_handlers = {
    yajlpp::pattern_property_handler(R"((?<filename>[^/]+))")
        .with_description("Map of file names to file state objects")
        .with_obj_provider<file_state, session_data_t>(
            [](const auto& ypc, session_data_t* root) {
                auto fn = ypc.get_substr("filename");
                return &root->sd_file_states[fn];
            })
        .with_children(file_state_handlers),
};

static const typed_json_path_container<session_data_t> view_info_handlers = {
    yajlpp::property_handler("save-time")
        .for_field(&session_data_t::sd_save_time),
    yajlpp::property_handler("time-offset")
        .for_field(&session_data_t::sd_time_offset),
    json_path_handler("files#", read_files),
    yajlpp::property_handler("file-states").with_children(file_states_handlers),
    yajlpp::property_handler("views").with_children(view_handlers),
};

void
load_session()
{
    static auto op = lnav_operation{"load_session"};

    auto op_guard = lnav_opid_guard::internal(op);
    log_info("BEGIN load_session");
    scan_sessions() | [](const auto pair) {
        lnav_data.ld_session_load_time = pair.first.second;
        const auto& view_info_path = pair.second;
        auto view_info_src = intern_string::lookup(view_info_path.string());

        auto open_res = lnav::filesystem::open_file(view_info_path, O_RDONLY);
        if (open_res.isErr()) {
            log_error("cannot open session file: %s -- %s",
                      view_info_path.c_str(),
                      open_res.unwrapErr().c_str());
            return;
        }

        auto fd = open_res.unwrap();
        unsigned char buffer[1024];
        ssize_t rc;

        log_info("loading session file: %s", view_info_path.c_str());
        auto parser = view_info_handlers.parser_for(view_info_src);
        while ((rc = read(fd, buffer, sizeof(buffer))) > 0) {
            auto buf_frag = string_fragment::from_bytes(buffer, rc);
            auto parse_res = parser.consume(buf_frag);
            if (parse_res.isErr()) {
                log_error("failed to load session: %s -- %s",
                          view_info_path.c_str(),
                          parse_res.unwrapErr()[0]
                              .to_attr_line()
                              .get_string()
                              .c_str());
                return;
            }
        }

        auto complete_res = parser.complete();
        if (complete_res.isErr()) {
            log_error("failed to load session: %s -- %s",
                      view_info_path.c_str(),
                      complete_res.unwrapErr()[0]
                          .to_attr_line()
                          .get_string()
                          .c_str());
            return;
        }
        session_data = complete_res.unwrap();

        bool log_changes = false;

        for (auto& lf : lnav_data.ld_active_files.fc_files) {
            auto iter = session_data.sd_file_states.find(lf->get_filename());

            if (iter == session_data.sd_file_states.end()) {
                continue;
            }

            log_debug("found state for file: %s %d (%s)",
                      lf->get_content_id().c_str(),
                      iter->second.fs_is_visible,
                      lf->get_filename_as_string().c_str());
            lnav_data.ld_log_source.find_data(lf) |
                [iter, &log_changes](auto ld) {
                    if (ld->ld_visible != iter->second.fs_is_visible) {
                        ld->get_file_ptr()->set_indexing(
                            iter->second.fs_is_visible);
                        ld->set_visibility(iter->second.fs_is_visible);
                        log_changes = true;
                    }
                };
        }

        if (log_changes) {
            lnav_data.ld_log_source.text_filters_changed();
        }
    };

    lnav::events::publish(lnav_data.ld_db.in(),
                          lnav::events::session::loaded{});

    log_info("END load_session");
}

static void
yajl_writer(void* context, const char* str, size_t len)
{
    FILE* file = (FILE*) context;

    fwrite(str, len, 1, file);
}

static void
save_user_bookmarks(sqlite3* db,
                    sqlite3_stmt* stmt,
                    bookmark_vector<content_line_t>& user_marks)
{
    auto& lss = lnav_data.ld_log_source;

    for (auto iter = user_marks.bv_tree.begin();
         iter != user_marks.bv_tree.end();
         ++iter)
    {
        content_line_t cl = *iter;
        auto lf = lss.find(cl);
        if (lf == nullptr) {
            continue;
        }

        sqlite3_clear_bindings(stmt);

        const auto line_iter = lf->begin() + cl;
        auto fr = lf->get_file_range(line_iter, false);
        auto read_result = lf->read_range(fr);

        if (read_result.isErr()) {
            continue;
        }

        auto line_hash = read_result
                             .map([cl](auto sbr) {
                                 return hasher()
                                     .update(sbr.get_data(), sbr.length())
                                     .update(cl)
                                     .to_string();
                             })
                             .unwrap();

        if (bind_values(stmt,
                        lf->original_line_time(line_iter),
                        lf->get_format()->get_name(),
                        line_hash,
                        lnav_data.ld_session_time)
            != SQLITE_OK)
        {
            continue;
        }

        if (sqlite3_bind_text(stmt, 5, "", 0, SQLITE_TRANSIENT) != SQLITE_OK) {
            log_error("could not bind log hash -- %s", sqlite3_errmsg(db));
            return;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("could not execute bookmark insert statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        marked_session_lines.emplace_back(lf->original_line_time(line_iter),
                                          lf->get_format_ptr()->get_name(),
                                          line_hash);

        sqlite3_reset(stmt);
    }
}

static void
save_meta_bookmarks(sqlite3* db, sqlite3_stmt* stmt, logfile* lf)
{
    for (const auto& bm_pair : lf->get_bookmark_metadata()) {
        auto cl = content_line_t(bm_pair.first);
        sqlite3_clear_bindings(stmt);

        auto line_iter = lf->begin() + cl;
        auto fr = lf->get_file_range(line_iter, false);
        auto read_result = lf->read_range(fr);

        if (read_result.isErr()) {
            continue;
        }

        auto line_hash = read_result
                             .map([cl](auto sbr) {
                                 return hasher()
                                     .update(sbr.get_data(), sbr.length())
                                     .update(cl)
                                     .to_string();
                             })
                             .unwrap();

        if (bind_values(stmt,
                        lf->original_line_time(line_iter),
                        lf->get_format()->get_name(),
                        line_hash,
                        lnav_data.ld_session_time)
            != SQLITE_OK)
        {
            continue;
        }

        const auto& line_meta = bm_pair.second;
        if (line_meta.empty(bookmark_metadata::categories::any)) {
            continue;
        }

        if (sqlite3_bind_text(stmt,
                              5,
                              line_meta.bm_name.c_str(),
                              line_meta.bm_name.length(),
                              SQLITE_TRANSIENT)
            != SQLITE_OK)
        {
            log_error("could not bind part name -- %s", sqlite3_errmsg(db));
            return;
        }

        if (sqlite3_bind_text(stmt,
                              6,
                              line_meta.bm_comment.c_str(),
                              line_meta.bm_comment.length(),
                              SQLITE_TRANSIENT)
            != SQLITE_OK)
        {
            log_error("could not bind comment -- %s", sqlite3_errmsg(db));
            return;
        }

        std::string tags;

        if (!line_meta.bm_tags.empty()) {
            yajlpp_gen gen;

            yajl_gen_config(gen, yajl_gen_beautify, false);

            {
                yajlpp_array arr(gen);

                for (const auto& str : line_meta.bm_tags) {
                    arr.gen(str);
                }
            }

            tags = gen.to_string_fragment().to_string();
        }

        if (sqlite3_bind_text(
                stmt, 7, tags.c_str(), tags.length(), SQLITE_TRANSIENT)
            != SQLITE_OK)
        {
            log_error("could not bind tags -- %s", sqlite3_errmsg(db));
            return;
        }

        if (!line_meta.bm_annotations.la_pairs.empty()) {
            auto anno_str = logmsg_annotations_handlers.to_string(
                line_meta.bm_annotations);

            if (sqlite3_bind_text(stmt,
                                  8,
                                  anno_str.c_str(),
                                  anno_str.length(),
                                  SQLITE_TRANSIENT)
                != SQLITE_OK)
            {
                log_error("could not bind annotations -- %s",
                          sqlite3_errmsg(db));
                return;
            }
        } else {
            sqlite3_bind_null(stmt, 8);
        }

        if (line_meta.bm_opid.empty()) {
            sqlite3_bind_null(stmt, 9);
        } else {
            bind_to_sqlite(stmt, 9, line_meta.bm_opid);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("could not execute bookmark insert statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        marked_session_lines.emplace_back(lf->original_line_time(line_iter),
                                          lf->get_format_ptr()->get_name(),
                                          line_hash);

        sqlite3_reset(stmt);
    }
}

static void
save_time_bookmarks()
{
    auto_sqlite3 db;
    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_mem<char, sqlite3_free> errmsg;
    auto_mem<sqlite3_stmt> stmt(sqlite3_finalize);

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        log_error("unable to open bookmark DB -- %s", db_path.c_str());
        return;
    }

    if (sqlite3_exec(db.in(), META_TABLE_DEF, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to make bookmark table -- %s", errmsg.in());
        return;
    }

    if (sqlite3_exec(
            db.in(), "BEGIN TRANSACTION", nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to begin transaction -- %s", errmsg.in());
        return;
    }

    {
        static const char* UPDATE_NETLOCS_STMT
            = R"(REPLACE INTO recent_netlocs (netloc) VALUES (?))";

        std::set<std::string> netlocs;

        isc::to<tailer::looper&, services::remote_tailer_t>().send_and_wait(
            [&netlocs](auto& tlooper) { netlocs = tlooper.active_netlocs(); });

        if (sqlite3_prepare_v2(
                db.in(), UPDATE_NETLOCS_STMT, -1, stmt.out(), nullptr)
            != SQLITE_OK)
        {
            log_error("could not prepare recent_netlocs statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        for (const auto& netloc : netlocs) {
            bind_to_sqlite(stmt.in(), 1, netloc);

            if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
                log_error("could not execute bookmark insert statement -- %s",
                          sqlite3_errmsg(db));
                return;
            }

            sqlite3_reset(stmt.in());
        }
        recent_refs.rr_netlocs.insert(netlocs.begin(), netlocs.end());
    }

    auto& lss = lnav_data.ld_log_source;
    auto& bm = lss.get_user_bookmarks();

    if (sqlite3_prepare_v2(db.in(),
                           "DELETE FROM bookmarks WHERE "
                           " log_time = ? and log_format = ? and log_hash = ? "
                           " and session_time = ?",
                           -1,
                           stmt.out(),
                           nullptr)
        != SQLITE_OK)
    {
        log_error("could not prepare bookmark delete statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    for (auto& marked_session_line : marked_session_lines) {
        sqlite3_clear_bindings(stmt.in());

        if (bind_values(stmt,
                        marked_session_line.sl_time,
                        marked_session_line.sl_format_name,
                        marked_session_line.sl_line_hash,
                        lnav_data.ld_session_time)
            != SQLITE_OK)
        {
            continue;
        }

        if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
            log_error("could not execute bookmark insert statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        sqlite3_reset(stmt.in());
    }

    marked_session_lines.clear();

    if (sqlite3_prepare_v2(db.in(),
                           "REPLACE INTO bookmarks"
                           " (log_time, log_format, log_hash, session_time, "
                           "part_name, comment, tags, annotations, log_opid)"
                           " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                           -1,
                           stmt.out(),
                           nullptr)
        != SQLITE_OK)
    {
        log_error("could not prepare bookmark replace statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    {
        for (auto file_iter = lnav_data.ld_log_source.begin();
             file_iter != lnav_data.ld_log_source.end();
             ++file_iter)
        {
            auto lf = (*file_iter)->get_file();

            if (lf == nullptr) {
                continue;
            }
            if (lf->size() == 0) {
                continue;
            }

            content_line_t base_content_line;
            base_content_line = lss.get_file_base_content_line(file_iter);
            base_content_line
                = content_line_t(base_content_line + lf->size() - 1);

            if (!bind_line(db.in(),
                           stmt.in(),
                           base_content_line,
                           lnav_data.ld_session_time))
            {
                continue;
            }

            if (sqlite3_bind_null(stmt.in(), 5) != SQLITE_OK) {
                log_error("could not bind log hash -- %s",
                          sqlite3_errmsg(db.in()));
                return;
            }

            if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
                log_error("could not execute bookmark insert statement -- %s",
                          sqlite3_errmsg(db));
                return;
            }

            sqlite3_reset(stmt.in());
        }
    }

    save_user_bookmarks(db.in(), stmt.in(), bm[&textview_curses::BM_USER]);
    for (const auto& ldd : lss) {
        auto* lf = ldd->get_file_ptr();
        if (lf == nullptr) {
            continue;
        }

        save_meta_bookmarks(db.in(), stmt.in(), lf);
    }

    if (sqlite3_prepare_v2(db.in(),
                           "DELETE FROM time_offset WHERE "
                           " log_time = ? and log_format = ? and log_hash = ? "
                           " and session_time = ?",
                           -1,
                           stmt.out(),
                           NULL)
        != SQLITE_OK)
    {
        log_error("could not prepare time_offset delete statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    for (auto& offset_session_line : offset_session_lines) {
        sqlite3_clear_bindings(stmt.in());

        if (bind_values(stmt,
                        offset_session_line.sl_time,
                        offset_session_line.sl_format_name,
                        offset_session_line.sl_line_hash,
                        lnav_data.ld_session_time)
            != SQLITE_OK)
        {
            continue;
        }

        if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
            log_error("could not execute bookmark insert statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        sqlite3_reset(stmt.in());
    }

    offset_session_lines.clear();

    if (sqlite3_prepare_v2(db.in(),
                           "REPLACE INTO time_offset"
                           " (log_time, log_format, log_hash, session_time, "
                           "offset_sec, offset_usec)"
                           " VALUES (?, ?, ?, ?, ?, ?)",
                           -1,
                           stmt.out(),
                           nullptr)
        != SQLITE_OK)
    {
        log_error("could not prepare time_offset replace statement -- %s",
                  sqlite3_errmsg(db));
        return;
    }

    {
        for (auto file_iter = lnav_data.ld_log_source.begin();
             file_iter != lnav_data.ld_log_source.end();
             ++file_iter)
        {
            auto lf = (*file_iter)->get_file();
            if (lf == nullptr) {
                continue;
            }
            if (lf->size() == 0) {
                continue;
            }

            if (bind_values(stmt,
                            lf->original_line_time(lf->begin()),
                            lf->get_format()->get_name(),
                            lf->get_content_id(),
                            lnav_data.ld_session_time)
                != SQLITE_OK)
            {
                continue;
            }

            if (sqlite3_bind_null(stmt.in(), 5) != SQLITE_OK) {
                log_error("could not bind log hash -- %s",
                          sqlite3_errmsg(db.in()));
                return;
            }

            if (sqlite3_bind_null(stmt.in(), 6) != SQLITE_OK) {
                log_error("could not bind log hash -- %s",
                          sqlite3_errmsg(db.in()));
                return;
            }

            if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
                log_error("could not execute bookmark insert statement -- %s",
                          sqlite3_errmsg(db));
                return;
            }

            sqlite3_reset(stmt.in());
        }
    }

    for (const auto& ls : lss) {
        if (ls->get_file() == nullptr) {
            continue;
        }

        const auto lf = ls->get_file();
        if (!lf->is_time_adjusted()) {
            continue;
        }

        auto line_iter = lf->begin() + lf->get_time_offset_line();
        auto offset = lf->get_time_offset();

        bind_values(stmt.in(),
                    lf->original_line_time(line_iter),
                    lf->get_format()->get_name(),
                    lf->get_content_id(),
                    lnav_data.ld_session_time,
                    offset.tv_sec,
                    offset.tv_usec);

        if (sqlite3_step(stmt.in()) != SQLITE_DONE) {
            log_error("could not execute bookmark insert statement -- %s",
                      sqlite3_errmsg(db));
            return;
        }

        sqlite3_reset(stmt.in());
    }

    if (sqlite3_exec(db.in(), "COMMIT", nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to begin transaction -- %s", errmsg.in());
        return;
    }

    if (sqlite3_exec(db.in(), BOOKMARK_LRU_STMT, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to delete old bookmarks -- %s", errmsg.in());
        return;
    }

    if (sqlite3_exec(db.in(), NETLOC_LRU_STMT, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to delete old netlocs -- %s", errmsg.in());
        return;
    }
}

static void
save_session_with_id(const std::string& session_id)
{
    auto_mem<FILE> file(fclose);
    yajl_gen handle = nullptr;

    /* TODO: save the last search query */

    log_info("saving session with id: %s", session_id.c_str());

    auto view_base_name
        = fmt::format(FMT_STRING("view-info-{}.ts{}.ppid{}.json"),
                      session_id,
                      lnav_data.ld_session_time,
                      getppid());
    auto view_file_name = lnav::paths::dotlnav() / view_base_name;
    auto view_file_tmp_name = view_file_name.string() + ".tmp";

    if ((file = fopen(view_file_tmp_name.c_str(), "w")) == nullptr) {
        perror("Unable to open session file");
    } else if (nullptr == (handle = yajl_gen_alloc(nullptr))) {
        perror("Unable to create yajl_gen object");
    } else {
        yajl_gen_config(
            handle, yajl_gen_print_callback, yajl_writer, file.in());

        {
            yajlpp_map root_map(handle);

            root_map.gen("save-time");
            root_map.gen((long long) time(nullptr));

            root_map.gen("time-offset");
            root_map.gen(lnav_data.ld_log_source.is_time_offset_enabled());

            root_map.gen("files");

            {
                yajlpp_array file_list(handle);

                for (auto& ld_file_name :
                     lnav_data.ld_active_files.fc_file_names)
                {
                    file_list.gen(ld_file_name.first);
                }
            }

            root_map.gen("file-states");

            {
                yajlpp_map file_states(handle);

                for (auto& lf : lnav_data.ld_active_files.fc_files) {
                    auto ld_opt = lnav_data.ld_log_source.find_data(lf);

                    file_states.gen(lf->get_filename().native());

                    {
                        yajlpp_map file_state(handle);

                        file_state.gen("visible");
                        file_state.gen(!ld_opt || ld_opt.value()->ld_visible);
                    }
                }
            }

            root_map.gen("views");

            {
                yajlpp_map top_view_map(handle);

                for (int lpc = 0; lpc < LNV__MAX; lpc++) {
                    auto& tc = lnav_data.ld_views[lpc];
                    auto* ta = dynamic_cast<text_anchors*>(tc.get_sub_source());

                    top_view_map.gen(lnav_view_strings[lpc]);

                    yajlpp_map view_map(handle);

                    view_map.gen("top_line");

                    if (tc.get_top() >= tc.get_top_for_last_row()) {
                        view_map.gen(-1LL);
                    } else {
                        view_map.gen((long long) tc.get_top());
                    }

                    if (tc.is_selectable() && tc.get_selection() >= 0_vl
                        && tc.get_inner_height() > 0_vl
                        && tc.get_selection() != tc.get_inner_height() - 1)
                    {
                        auto sel = tc.get_selection();
                        if (sel) {
                            view_map.gen("focused_line");
                            view_map.gen((long long) sel.value());

                            if (ta != nullptr) {
                                auto anchor_opt
                                    = ta->anchor_for_row(sel.value());
                                if (anchor_opt) {
                                    view_map.gen("anchor");
                                    view_map.gen(anchor_opt.value());
                                }
                            }
                        }
                    }

                    view_map.gen("search");
                    view_map.gen(lnav_data.ld_views[lpc].get_current_search());

                    view_map.gen("word_wrap");
                    view_map.gen(tc.get_word_wrap());

                    auto tss = tc.get_sub_source();
                    if (tss == nullptr) {
                        continue;
                    }

                    view_map.gen("filtering");
                    view_map.gen(tss->tss_apply_filters);

                    auto& fs = tss->get_filters();

                    view_map.gen("commands");
                    yajlpp_array cmd_array(handle);

                    for (const auto& filter : fs) {
                        auto cmd = filter->to_command();

                        if (cmd.empty()) {
                            continue;
                        }

                        cmd_array.gen(cmd);

                        if (!filter->is_enabled()) {
                            cmd_array.gen("disable-filter " + filter->get_id());
                        }
                    }

                    auto& hmap = lnav_data.ld_views[lpc].get_highlights();

                    for (auto& hl : hmap) {
                        if (hl.first.first != highlight_source_t::INTERACTIVE) {
                            continue;
                        }
                        cmd_array.gen("highlight " + hl.first.second);
                    }

                    auto* ttt = dynamic_cast<text_time_translator*>(tss);
                    if (ttt != nullptr) {
                        for (const auto& format :
                             log_format::get_root_formats())
                        {
                            auto field_states = format->get_field_states();

                            for (const auto& fs_pair : field_states) {
                                if (!fs_pair.second.lvm_user_hidden) {
                                    continue;
                                }

                                if (fs_pair.second.lvm_user_hidden.value()) {
                                    cmd_array.gen(
                                        "hide-fields "
                                        + format->get_name().to_string() + "."
                                        + fs_pair.first.to_string());
                                } else if (fs_pair.second.lvm_hidden) {
                                    cmd_array.gen(
                                        "show-fields "
                                        + format->get_name().to_string() + "."
                                        + fs_pair.first.to_string());
                                }
                            }
                        }

                        auto min_time_opt = ttt->get_min_row_time();
                        auto max_time_opt = ttt->get_max_row_time();
                        char min_time_str[32], max_time_str[32];

                        if (min_time_opt) {
                            sql_strftime(min_time_str,
                                         sizeof(min_time_str),
                                         min_time_opt.value());
                            cmd_array.gen("hide-lines-before "
                                          + std::string(min_time_str));
                        }
                        if (max_time_opt) {
                            sql_strftime(max_time_str,
                                         sizeof(max_time_str),
                                         max_time_opt.value());
                            cmd_array.gen("hide-lines-after "
                                          + std::string(max_time_str));
                        }

                        if (lpc == LNV_LOG) {
                            auto& lss = lnav_data.ld_log_source;
                            auto mark_expr = lss.get_sql_marker_text();
                            if (!mark_expr.empty()) {
                                cmd_array.gen("mark-expr " + mark_expr);
                            }
                        }
                    }
                }
            }
        }

        yajl_gen_clear(handle);
        yajl_gen_free(handle);

        fclose(file.release());

        log_perror(rename(view_file_tmp_name.c_str(), view_file_name.c_str()));

        log_info("Saved session: %s", view_file_name.c_str());
    }
}

void
save_session()
{
    if (lnav_data.ld_flags & LNF_SECURE_MODE) {
        log_info("secure mode is enabled, not saving session");
        return;
    }

    static auto op = lnav_operation{"save_session"};

    auto op_guard = lnav_opid_guard::internal(op);

    log_debug("BEGIN save_session");
    save_time_bookmarks();

    const auto opt_session_id = compute_session_id();
    opt_session_id | [](auto& session_id) { save_session_with_id(session_id); };
    for (const auto& pair : lnav_data.ld_session_id) {
        if (opt_session_id && pair.first == opt_session_id.value()) {
            continue;
        }
        save_session_with_id(pair.first);
    }
    log_debug("END save_session");
}

void
reset_session()
{
    log_info("reset session: time=%d", lnav_data.ld_session_time);

    save_session();

    lnav_data.ld_session_time = time(nullptr);
    session_data.sd_file_states.clear();

    for (auto& tc : lnav_data.ld_views) {
        auto* ttt = dynamic_cast<text_time_translator*>(tc.get_sub_source());
        auto& hmap = tc.get_highlights();
        auto hl_iter = hmap.begin();

        while (hl_iter != hmap.end()) {
            if (hl_iter->first.first != highlight_source_t::INTERACTIVE) {
                ++hl_iter;
            } else {
                hmap.erase(hl_iter++);
            }
        }

        if (ttt != nullptr) {
            ttt->clear_min_max_row_times();
        }
    }

    for (const auto& lf : lnav_data.ld_active_files.fc_files) {
        lf->reset_state();
    }

    // XXX clean this up
    lnav_data.ld_log_source.set_force_rebuild();
    lnav_data.ld_log_source.set_marked_only(false);
    lnav_data.ld_log_source.set_min_log_level(LEVEL_UNKNOWN);
    lnav_data.ld_log_source.set_sql_filter("", nullptr);
    lnav_data.ld_log_source.set_sql_marker("", nullptr);
    lnav_data.ld_log_source.clear_bookmark_metadata();
    rebuild_indexes(std::nullopt);

    lnav_data.ld_db_row_source.reset_user_state();

    for (auto& tc : lnav_data.ld_views) {
        text_sub_source* tss = tc.get_sub_source();

        if (tss == nullptr) {
            continue;
        }
        tss->get_filters().clear_filters();
        tss->tss_apply_filters = true;
        tss->text_filters_changed();
        tss->text_clear_marks(&textview_curses::BM_USER);
        tc.get_bookmarks()[&textview_curses::BM_USER].clear();
        tss->text_clear_marks(&textview_curses::BM_META);
        tc.get_bookmarks()[&textview_curses::BM_META].clear();
        tc.reload_data();
    }

    lnav_data.ld_filter_view.reload_data();
    lnav_data.ld_files_view.reload_data();
    for (const auto& format : log_format::get_root_formats()) {
        auto* elf = dynamic_cast<external_log_format*>(format.get());

        if (elf == nullptr) {
            continue;
        }

        bool changed = false;
        for (const auto& vd : elf->elf_value_defs) {
            if (vd.second->vd_meta.lvm_user_hidden) {
                vd.second->vd_meta.lvm_user_hidden = std::nullopt;
                changed = true;
            }
        }
        if (changed) {
            elf->elf_value_defs_state->vds_generation += 1;
        }
    }
}

void
lnav::session::restore_view_states()
{
    static auto op = lnav_operation{__FUNCTION__};

    auto op_guard = lnav_opid_guard::internal(op);

    log_debug("restoring view states");
    for (size_t view_index = 0; view_index < LNV__MAX; view_index++) {
        const auto& vs = session_data.sd_view_states[view_index];
        auto& tview = lnav_data.ld_views[view_index];
        auto* ta = dynamic_cast<text_anchors*>(tview.get_sub_source());
        bool has_loc = false;

        if (view_index == LNV_TEXT) {
            auto lf = lnav_data.ld_text_source.current_file();
            if (lf != nullptr) {
                const auto& init_loc = lf->get_open_options().loo_init_location;
                has_loc = init_loc.valid() || init_loc.is<file_location_tail>();
                if (!has_loc) {
                    switch (lf->get_text_format()) {
                        case text_format_t::TF_UNKNOWN:
                        case text_format_t::TF_LOG:
                            break;
                        default:
                            if (vs.vs_top == 0 && tview.get_top() > 0) {
                                log_debug("setting to 0");
                                tview.set_top(0_vl);
                            }
                            break;
                    }
                }
            }
        } else if (view_index == LNV_LOG) {
            file_location_t log_loc{mapbox::util::no_init{}};
            for (const auto& ld : lnav_data.ld_log_source) {
                const auto* lf = ld->get_file_ptr();
                if (lf == nullptr) {
                    continue;
                }

                const auto& init_loc = lf->get_open_options().loo_init_location;
                if (init_loc.valid() && init_loc.is<std::string>()) {
                    has_loc = true;
                    log_loc = init_loc;
                }
            }

            if (log_loc.valid()) {
                auto anchor = log_loc.get<std::string>();
                auto row_opt = lnav_data.ld_log_source.row_for_anchor(anchor);
                if (row_opt) {
                    log_info("setting LOG view to desired loc: %s",
                             anchor.c_str());
                    tview.set_selection(row_opt.value());
                }
            }
        }

        if (!vs.vs_search.empty()) {
            tview.execute_search(vs.vs_search);
            tview.set_follow_search_for(-1, {});
        }
        tview.set_word_wrap(vs.vs_word_wrap);
        if (tview.get_sub_source() != nullptr) {
            tview.get_sub_source()->tss_apply_filters = vs.vs_filtering;
        }
        for (const auto& cmdline : vs.vs_commands) {
            auto active = ensure_view(&tview);
            auto exec_cmd_res
                = execute_command(lnav_data.ld_exec_context, cmdline);
            if (exec_cmd_res.isOk()) {
                log_info("Result: %s", exec_cmd_res.unwrap().c_str());
            } else {
                log_error("Result: %s",
                          exec_cmd_res.unwrapErr()
                              .to_attr_line()
                              .get_string()
                              .c_str());
            }
            if (!active) {
                lnav_data.ld_view_stack.pop_back();
                lnav_data.ld_view_stack.top() | [](auto* tc) {
                    // XXX
                    if (tc == &lnav_data.ld_views[LNV_TIMELINE]) {
                        auto tss = tc->get_sub_source();
                        tss->text_filters_changed();
                        tc->reload_data();
                    }
                };
            }
        }

        if (!has_loc && vs.vs_top >= 0
            && (view_index == LNV_LOG || tview.get_top() == 0_vl
                || tview.get_top() == tview.get_top_for_last_row()))
        {
            log_info("restoring %s view top: %d",
                     lnav_view_strings[view_index].data(),
                     vs.vs_top);
            tview.set_top(vis_line_t(vs.vs_top), true);
            tview.set_selection(-1_vl);
        }
        if (!has_loc && vs.vs_selection) {
            log_info("restoring %s view selection: %d",
                     lnav_view_strings[view_index].data(),
                     vs.vs_selection.value());
            tview.set_selection(vis_line_t(vs.vs_selection.value()));
        }
        auto sel = tview.get_selection();
        if (!has_loc && sel && ta != nullptr && vs.vs_anchor
            && !vs.vs_anchor->empty())
        {
            auto curr_anchor = ta->anchor_for_row(sel.value());

            if (!curr_anchor || curr_anchor.value() != vs.vs_anchor.value()) {
                log_info("%s view anchor mismatch %s != %s",
                         lnav_view_strings[view_index].data(),
                         curr_anchor.value_or("").c_str(),
                         vs.vs_anchor.value().c_str());

                auto row_opt = ta->row_for_anchor(vs.vs_anchor.value());
                if (row_opt) {
                    tview.set_selection(row_opt.value());
                }
            }
        }
        sel = tview.get_selection();
        if (!sel && tview.get_top() == 0_vl && tview.listview_rows(tview) > 0) {
            tview.set_selection(0_vl);
        }
        log_info("%s view actual top/selection: %d/%d",
                 lnav_view_strings[view_index].data(),
                 tview.get_top(),
                 tview.get_selection().value_or(-1_vl));
    }
}

void
lnav::session::regex101::insert_entry(const lnav::session::regex101::entry& ei)
{
    constexpr const char* STMT = R"(
       INSERT INTO regex101_entries
          (format_name, regex_name, permalink, delete_code)
          VALUES (?, ?, ?, ?);
)";

    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_sqlite3 db;

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        return;
    }

    auto_mem<char, sqlite3_free> errmsg;
    if (sqlite3_exec(db.in(), META_TABLE_DEF, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to make bookmark table -- %s", errmsg.in());
        return;
    }

    auto prep_res = prepare_stmt(db.in(),
                                 STMT,
                                 ei.re_format_name,
                                 ei.re_regex_name,
                                 ei.re_permalink,
                                 ei.re_delete_code);

    if (prep_res.isErr()) {
        return;
    }

    auto ps = prep_res.unwrap();

    ps.execute();
}

template<>
struct from_sqlite<lnav::session::regex101::entry> {
    inline lnav::session::regex101::entry operator()(int argc,
                                                     sqlite3_value** argv,
                                                     int argi)
    {
        return {
            from_sqlite<std::string>()(argc, argv, argi + 0),
            from_sqlite<std::string>()(argc, argv, argi + 1),
            from_sqlite<std::string>()(argc, argv, argi + 2),
            from_sqlite<std::string>()(argc, argv, argi + 3),
        };
    }
};

Result<std::vector<lnav::session::regex101::entry>, std::string>
lnav::session::regex101::get_entries()
{
    constexpr const char* STMT = R"(
       SELECT * FROM regex101_entries;
)";

    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_sqlite3 db;

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        return Err(std::string());
    }

    auto_mem<char, sqlite3_free> errmsg;
    if (sqlite3_exec(db.in(), META_TABLE_DEF, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to make bookmark table -- %s", errmsg.in());
        return Err(std::string(errmsg));
    }

    auto ps = TRY(prepare_stmt(db.in(), STMT));
    bool done = false;
    std::vector<entry> retval;

    while (!done) {
        auto fetch_res = ps.fetch_row<entry>();

        if (fetch_res.is<prepared_stmt::fetch_error>()) {
            return Err(fetch_res.get<prepared_stmt::fetch_error>().fe_msg);
        }

        fetch_res.match(
            [&done](const prepared_stmt::end_of_rows&) { done = true; },
            [](const prepared_stmt::fetch_error&) {},
            [&retval](entry en) { retval.emplace_back(en); });
    }
    return Ok(retval);
}

void
lnav::session::regex101::delete_entry(const std::string& format_name,
                                      const std::string& regex_name)
{
    constexpr const char* STMT = R"(
       DELETE FROM regex101_entries WHERE
          format_name = ? AND regex_name = ?;
)";

    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_sqlite3 db;

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        return;
    }

    auto prep_res = prepare_stmt(db.in(), STMT, format_name, regex_name);

    if (prep_res.isErr()) {
        return;
    }

    auto ps = prep_res.unwrap();

    ps.execute();
}

lnav::session::regex101::get_result_t
lnav::session::regex101::get_entry(const std::string& format_name,
                                   const std::string& regex_name)
{
    constexpr const char* STMT = R"(
       SELECT * FROM regex101_entries WHERE
          format_name = ? AND regex_name = ?;
    )";

    auto db_path = lnav::paths::dotlnav() / LOG_METADATA_NAME;
    auto_sqlite3 db;

    if (sqlite3_open(db_path.c_str(), db.out()) != SQLITE_OK) {
        return error{std::string()};
    }

    auto_mem<char, sqlite3_free> errmsg;
    if (sqlite3_exec(db.in(), META_TABLE_DEF, nullptr, nullptr, errmsg.out())
        != SQLITE_OK)
    {
        log_error("unable to make bookmark table -- %s", errmsg.in());
        return error{std::string(errmsg)};
    }

    auto prep_res = prepare_stmt(db.in(), STMT, format_name, regex_name);
    if (prep_res.isErr()) {
        return error{prep_res.unwrapErr()};
    }

    auto ps = prep_res.unwrap();
    return ps.fetch_row<entry>().match(
        [](const prepared_stmt::fetch_error& fe) -> get_result_t {
            return error{fe.fe_msg};
        },
        [](const prepared_stmt::end_of_rows&) -> get_result_t {
            return no_entry{};
        },
        [](const entry& en) -> get_result_t { return en; });
}
