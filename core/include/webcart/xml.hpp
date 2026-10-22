#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace webcart::xml {

std::string escape(std::string_view text);

// The gateway protocol only ever sends a single empty element with
// attributes, e.g. <request op="addItem" cart="C1" sku="TEA-001" qty="2"/>.
// This parser accepts exactly that (plus an optional XML declaration and
// surrounding whitespace) and rejects everything else.
struct Element {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::string attr(const std::string& key, const std::string& fallback = "") const;
};

Element parse_single_element(std::string_view document);

// Small streaming writer for responses.
class Writer {
public:
    Writer& open(std::string_view name);
    Writer& attr(std::string_view key, std::string_view value);
    Writer& attr(std::string_view key, long long value);
    Writer& text(std::string_view value);
    Writer& close();  // closes the innermost element, self-closing if empty
    std::string str();

private:
    struct Frame {
        std::string name;
        bool has_content = false;
    };
    void end_start_tag();
    std::string out_;
    std::vector<Frame> stack_;
    bool start_tag_open_ = false;
};

}  // namespace webcart::xml
