#include "webcart/xml.hpp"

#include <cctype>

#include "webcart/errors.hpp"

namespace webcart::xml {

std::string escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:
                // Control characters other than tab/CR/LF are not legal XML 1.0.
                if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r')
                    out += '?';
                else
                    out += c;
        }
    }
    return out;
}

std::string Element::attr(const std::string& key, const std::string& fallback) const {
    const auto it = attributes.find(key);
    return it == attributes.end() ? fallback : it->second;
}

namespace {

[[noreturn]] void fail(const std::string& why) {
    throw ShopError(ErrorCode::BadRequest, "malformed request: " + why);
}

bool is_name_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.' ||
           c == ':';
}

std::string unescape(std::string_view s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') {
            out += s[i];
            continue;
        }
        const auto semi = s.find(';', i);
        if (semi == std::string_view::npos) fail("unterminated entity");
        const std::string_view ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (ent.size() > 1 && ent[0] == '#') {
            unsigned long code = 0;
            try {
                code = ent[1] == 'x' ? std::stoul(std::string(ent.substr(2)), nullptr, 16)
                                     : std::stoul(std::string(ent.substr(1)));
            } catch (const std::exception&) {
                fail("bad character reference");
            }
            if (code == 0 || code > 0x10FFFF) fail("bad character reference");
            // UTF-8 encode
            if (code < 0x80) {
                out += static_cast<char>(code);
            } else if (code < 0x800) {
                out += static_cast<char>(0xC0 | (code >> 6));
                out += static_cast<char>(0x80 | (code & 0x3F));
            } else if (code < 0x10000) {
                out += static_cast<char>(0xE0 | (code >> 12));
                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (code >> 18));
                out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
        } else {
            fail("unknown entity &" + std::string(ent) + ";");
        }
        i = semi;
    }
    return out;
}

}  // namespace

Element parse_single_element(std::string_view doc) {
    std::size_t i = 0;
    auto skip_ws = [&] {
        while (i < doc.size() && std::isspace(static_cast<unsigned char>(doc[i]))) ++i;
    };
    // Optional UTF-8 byte order mark and XML declaration.
    if (doc.substr(0, 3) == "\xEF\xBB\xBF") i = 3;
    skip_ws();
    if (doc.substr(i, 5) == "<?xml") {
        const auto end = doc.find("?>", i);
        if (end == std::string_view::npos) fail("unterminated declaration");
        i = end + 2;
        skip_ws();
    }
    if (i >= doc.size() || doc[i] != '<') fail("expected '<'");
    ++i;
    Element el;
    while (i < doc.size() && is_name_char(doc[i])) el.name += doc[i++];
    if (el.name.empty()) fail("missing element name");

    for (;;) {
        skip_ws();
        if (i >= doc.size()) fail("unterminated element");
        if (doc[i] == '/') {
            if (i + 1 >= doc.size() || doc[i + 1] != '>') fail("expected '/>'");
            i += 2;
            break;
        }
        if (doc[i] == '>') {
            // Allow <request ...></request> with only whitespace inside.
            ++i;
            skip_ws();
            const std::string closing = "</" + el.name;
            if (doc.substr(i, closing.size()) != closing) fail("element content not supported");
            i += closing.size();
            skip_ws();
            if (i >= doc.size() || doc[i] != '>') fail("bad closing tag");
            ++i;
            break;
        }
        std::string key;
        while (i < doc.size() && is_name_char(doc[i])) key += doc[i++];
        if (key.empty()) fail("expected attribute name");
        skip_ws();
        if (i >= doc.size() || doc[i] != '=') fail("expected '=' after " + key);
        ++i;
        skip_ws();
        if (i >= doc.size() || (doc[i] != '"' && doc[i] != '\'')) fail("expected quoted value");
        const char quote = doc[i++];
        const auto end = doc.find(quote, i);
        if (end == std::string_view::npos) fail("unterminated attribute value");
        const std::string_view raw = doc.substr(i, end - i);
        if (raw.find('<') != std::string_view::npos) fail("'<' in attribute value");
        if (!el.attributes.emplace(key, unescape(raw)).second) fail("duplicate attribute " + key);
        i = end + 1;
    }
    skip_ws();
    if (i != doc.size()) fail("trailing content after root element");
    return el;
}

void Writer::end_start_tag() {
    if (start_tag_open_) {
        out_ += '>';
        start_tag_open_ = false;
    }
}

Writer& Writer::open(std::string_view name) {
    end_start_tag();
    if (!stack_.empty()) stack_.back().has_content = true;
    out_ += '<';
    out_ += name;
    stack_.push_back({std::string(name), false});
    start_tag_open_ = true;
    return *this;
}

Writer& Writer::attr(std::string_view key, std::string_view value) {
    out_ += ' ';
    out_ += key;
    out_ += "=\"";
    out_ += escape(value);
    out_ += '"';
    return *this;
}

Writer& Writer::attr(std::string_view key, long long value) {
    return attr(key, std::to_string(value));
}

Writer& Writer::text(std::string_view value) {
    end_start_tag();
    stack_.back().has_content = true;
    out_ += escape(value);
    return *this;
}

Writer& Writer::close() {
    const Frame frame = stack_.back();
    stack_.pop_back();
    if (!frame.has_content) {
        out_ += "/>";
        start_tag_open_ = false;
    } else {
        end_start_tag();
        out_ += "</" + frame.name + ">";
    }
    return *this;
}

std::string Writer::str() {
    while (!stack_.empty()) close();
    return out_;
}

}  // namespace webcart::xml
