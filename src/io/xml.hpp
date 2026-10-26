#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tisma {

class XmlError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Reads the elements of an XML document one at a time (the part of XML 1.0
// that model files use): start and end tags with their attributes; text,
// comments, processing instructions, CDATA and the document type are
// skipped. An empty element tag gives a start and then an end. Names keep
// their prefix (local_name drops it). Attribute values are as written: decode
// turns entity and character references into text. Throws XmlError. The
// text must outlive the reader.
class XmlReader {
public:
    enum class Event { Start, End, Done };

    explicit XmlReader(std::string_view text) : text_(text) {}

    Event next();
    std::string_view name() const { return name_; }
    // The value of the attribute with this local name, as written.
    std::optional<std::string_view> attribute(std::string_view local) const;
    std::size_t line() const; // of the current tag, for messages

    static std::string_view local_name(std::string_view name);
    static std::string decode(std::string_view raw);

private:
    [[noreturn]] void fail(const std::string& message) const;
    void skip_past(std::string_view end);

    std::string_view text_;
    std::size_t pos_ = 0, tag_ = 0;
    std::string_view name_;
    std::vector<std::pair<std::string_view, std::string_view>> attributes_;
    bool pending_end_ = false;
    int depth_ = 0;
};

} // namespace tisma
