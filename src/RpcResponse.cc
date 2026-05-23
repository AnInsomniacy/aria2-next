/* <!-- copyright */
/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2009 Tatsuhiro Tsujikawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
/* copyright --> */
#include "RpcResponse.h"

#include <cassert>
#include <sstream>

#include "BoostJsonValue.h"
#include "util.h"
#ifdef HAVE_ZLIB
#  include "GZipEncoder.h"
#endif // HAVE_ZLIB

namespace aria2 {

namespace rpc {

RpcResponse::RpcResponse(int code, RpcResponse::authorization_t authorized,
                         std::unique_ptr<ValueBase> param,
                         std::unique_ptr<ValueBase> id)
    : param{std::move(param)},
      id{std::move(id)},
      code{code},
      authorized{authorized}
{
}

namespace {
template <typename OutputStream>
OutputStream& encodeJsonAll(OutputStream& o, int code, const ValueBase* param,
                            const ValueBase* id,
                            const std::string& callback = A2STR::NIL)
{
  if (!callback.empty()) {
    o << callback << "(";
  }
  o << "{\"id\":";
  o << json::serialize(id);
  o << ",\"jsonrpc\":\"2.0\",";
  if (code == 0) {
    o << "\"result\":";
  }
  else {
    o << "\"error\":";
  }
  o << json::serialize(param);
  o << "}";
  if (!callback.empty()) {
    o << ")";
  }
  return o;
}
} // namespace

std::string toJson(const RpcResponse& res, const std::string& callback,
                   bool gzip)
{
  if (gzip) {
#ifdef HAVE_ZLIB
    GZipEncoder o;
    o.init();
    return encodeJsonAll(o, res.code, res.param.get(), res.id.get(), callback)
        .str();
#else  // !HAVE_ZLIB
    abort();
#endif // !HAVE_ZLIB
  }
  else {
    std::stringstream o;
    return encodeJsonAll(o, res.code, res.param.get(), res.id.get(), callback)
        .str();
  }
}

namespace {
template <typename OutputStream>
OutputStream& encodeJsonBatchAll(OutputStream& o,
                                 const std::vector<RpcResponse>& results,
                                 const std::string& callback)
{
  if (!callback.empty()) {
    o << callback << "(";
  }
  o << "[";
  if (!results.empty()) {
    encodeJsonAll(o, results[0].code, results[0].param.get(),
                  results[0].id.get());

    for (auto i = std::begin(results) + 1, eoi = std::end(results); i != eoi;
         ++i) {
      o << ",";
      encodeJsonAll(o, (*i).code, (*i).param.get(), (*i).id.get());
    }
  }
  o << "]";
  if (!callback.empty()) {
    o << ")";
  }
  return o;
}
} // namespace

std::string toJsonBatch(const std::vector<RpcResponse>& results,
                        const std::string& callback, bool gzip)
{
  if (gzip) {
#ifdef HAVE_ZLIB
    GZipEncoder o;
    o.init();
    return encodeJsonBatchAll(o, results, callback).str();
#else  // !HAVE_ZLIB
    abort();
#endif // !HAVE_ZLIB
  }
  else {
    std::stringstream o;
    return encodeJsonBatchAll(o, results, callback).str();
  }
}

} // namespace rpc

} // namespace aria2
