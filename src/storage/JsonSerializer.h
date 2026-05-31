#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <map>

namespace dbms {

class JsonWriter {
public:
    JsonWriter() : first_(true) {}

    JsonWriter& add(const std::string& key, const std::string& val) {
        comma();
        ss_ << "\"" << escape(key) << "\":\"" << escape(val) << "\"";
        return *this;
    }

    JsonWriter& add(const std::string& key, bool val) {
        comma();
        ss_ << "\"" << escape(key) << "\":" << (val ? "true" : "false");
        return *this;
    }

    JsonWriter& add(const std::string& key, int val) {
        comma();
        ss_ << "\"" << escape(key) << "\":" << val;
        return *this;
    }

    JsonWriter& start_array(const std::string& key) {
        comma();
        ss_ << "\"" << escape(key) << "\":[";
        first_ = true;
        return *this;
    }

    JsonWriter& end_array() {
        ss_ << "]";
        return *this;
    }

    JsonWriter& start_object() {
        comma();
        ss_ << "{";
        first_ = true;
        return *this;
    }

    JsonWriter& end_object() {
        ss_ << "}";
        return *this;
    }

    std::string str() const {
        return ss_.str();
    }

    static std::string error(const std::string& msg) {
        JsonWriter w;
        w.ss_ << "{";
        w.add("ok", false);
        w.add("msg", msg);
        w.ss_ << "}";
        return w.str();
    }

    static std::string ok(const std::string& msg) {
        JsonWriter w;
        w.ss_ << "{";
        w.add("ok", true);
        w.add("msg", msg);
        w.ss_ << "}";
        return w.str();
    }

private:
    void comma() {
        if (!first_) ss_ << ",";
        first_ = false;
    }

    static std::string escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c;
            }
        }
        return out;
    }

    std::stringstream ss_;
    bool first_;
};

// Minimal JSON parser for simple objects used in wire format
class JsonReader {
public:
    JsonReader(const std::string& json) : json_(json) {}

    bool get_bool(const std::string& key, bool default_val = false) const {
        std::string val = get_raw(key);
        if (val.empty()) return default_val;
        return val == "true";
    }

    std::string get_string(const std::string& key) const {
        std::string val = get_raw(key);
        // Strip surrounding quotes if present
        if (val.size() >= 2 && val.front() == '\"' && val.back() == '\"') {
            val = val.substr(1, val.size() - 2);
        }
        return unescape(val);
    }

private:
    std::string get_raw(const std::string& key) const {
        size_t start = json_.find("\"" + key + "\"");
        if (start == std::string::npos) return "";

        size_t colon = json_.find(':', start);
        if (colon == std::string::npos) return "";

        colon++;
        while (colon < json_.size() && (json_[colon] == ' ' || json_[colon] == '\t'))
            colon++;

        if (colon >= json_.size()) return "";

        if (json_[colon] == '\"') {
            // String value
            colon++;
            std::string val;
            while (colon < json_.size() && json_[colon] != '\"') {
                val += json_[colon];
                colon++;
            }
            return val;
        } else {
            // Unquoted value (bool, number)
            std::string val;
            while (colon < json_.size() && json_[colon] != ',' && json_[colon] != '}' && json_[colon] != ']') {
                if (json_[colon] != ' ' && json_[colon] != '\t' && json_[colon] != '\n')
                    val += json_[colon];
                colon++;
            }
            return val;
        }
    }

    static std::string unescape(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                switch (s[i + 1]) {
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += s[i + 1];
                }
                i++;
            } else {
                out += s[i];
            }
        }
        return out;
    }

private:
    std::string json_;
};

} // namespace dbms
