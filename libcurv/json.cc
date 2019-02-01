// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#include <libcurv/json.h>

#include <libcurv/dtostr.h>
#include <libcurv/list.h>
#include <libcurv/record.h>

namespace curv {

void write_json_string(const char* str, std::ostream& out)
{
    out << '"';
    for (const char* p = str; *p != '\0'; ++p) {
        // In the JSON-API protocol, top level objects are separated by
        // newlines, and for ease of parsing by the client, top level objects
        // cannot contain raw newlines. So newlines are encoded as \n in
        // JSON strings.
        if (*p == '\n')
            out << "\\n";
        else {
            if (*p == '\\' || *p == '"')
                out << '\\';
            out << *p;
        }
    }
    out << '"';
}

void write_json_value(Value val, std::ostream& out)
{
    if (val.is_null()) {
        out << "null";
        return;
    }
    if (val.is_bool()) {
        out << val;
        return;
    }
    if (val.is_num()) {
        out << dfmt(val.get_num_unsafe(), dfmt::JSON);
        return;
    }
    assert(val.is_ref());
    auto& ref = val.get_ref_unsafe();
    switch (ref.type_) {
    case Ref_Value::ty_string:
      {
        auto& str = (String&)ref;
        write_json_string(str.c_str(), out);
        return;
      }
    case Ref_Value::ty_list:
      {
        auto& list = (List&)ref;
        out << "[";
        bool first = true;
        for (auto e : list) {
            if (!first) out << ",";
            first = false;
            write_json_value(e, out);
        }
        out << "]";
        return;
      }
    case Ref_Value::ty_record:
      {
        auto& record = (Record&)ref;
        out << "{";
        bool first = true;
        for (auto f = record.iter(); !f->empty(); f->next()) {
            if (!first) out << ",";
            first = false;
            write_json_string(f->key().c_str(), out);
            out << ":";
            Value fval = f->maybe_value();
            if (fval.eq(missing)) {
                out << "{\"\\0\":\"\"}";
            } else {
                write_json_value(fval, out);
            }
        }
        out << "}";
        return;
      }
    default:
        out << "{\"\\0\":\"" << val << "\"}";
        return;
    }
}

} // namespace curv
