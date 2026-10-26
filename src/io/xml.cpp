// A small XML reader for model files. Own implementation of the part of XML
// 1.0 (W3C) they use; see PROVENANCE.md.

#include "io/xml.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>

namespace tisma {
namespace {

bool space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool name_end(char c) { return space(c) || c == '/' || c == '>' || c == '='; }

void put_utf8(std::string& out, std::uint32_t c) {
    if (c < 0x80) {
        out.push_back(char(c));
    } else if (c < 0x800) {
        out.push_back(char(0xC0 | (c >> 6)));
        out.push_back(char(0x80 | (c & 0x3F)));
    } else if (c < 0x10000) {
        out.push_back(char(0xE0 | (c >> 12)));
        out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(char(0x80 | (c & 0x3F)));
    } else {
        out.push_back(char(0xF0 | (c >> 18)));
        out.push_back(char(0x80 | ((c >> 12) & 0x3F)));
        out.push_back(char(0x80 | ((c >> 6) & 0x3F)));
        out.push_back(char(0x80 | (c & 0x3F)));
    }
}

} // namespace

std::size_t XmlReader::line() const {
    return 1 + std::size_t(std::count(text_.begin(), text_.begin() + std::ptrdiff_t(std::min(tag_, text_.size())), '\n'));
}

void XmlReader::fail(const std::string& message) const {
    throw XmlError("XML, line " + std::to_string(line()) + ": " + message);
}

void XmlReader::skip_past(std::string_view end) {
    const std::size_t at = text_.find(end, pos_);
    if (at == std::string_view::npos)
        fail("unterminated markup");
    pos_ = at + end.size();
}

XmlReader::Event XmlReader::next() {
    if (pending_end_) {
        pending_end_ = false;
        attributes_.clear();
        --depth_;
        return Event::End;
    }
    for (;;) {
        pos_ = text_.find('<', pos_);
        if (pos_ == std::string_view::npos) {
            if (depth_ != 0) {
                tag_ = text_.size();
                fail("the document ends inside an element");
            }
            return Event::Done;
        }
        tag_ = pos_;
        const std::string_view rest = text_.substr(pos_);
        if (rest.starts_with("<?")) {
            skip_past("?>");
        } else if (rest.starts_with("<!--")) {
            skip_past("-->");
        } else if (rest.starts_with("<![CDATA[")) {
            skip_past("]]>");
        } else if (rest.starts_with("<!")) {
            // A document type, with its internal subset if it has one.
            const std::size_t bracket = text_.find('[', pos_), close = text_.find('>', pos_);
            skip_past(bracket != std::string_view::npos && bracket < close ? "]>" : ">");
        } else {
            break;
        }
    }
    attributes_.clear();
    const bool end = pos_ + 1 < text_.size() && text_[pos_ + 1] == '/';
    pos_ += end ? 2 : 1;
    const std::size_t start = pos_;
    while (pos_ < text_.size() && !name_end(text_[pos_]))
        ++pos_;
    name_ = text_.substr(start, pos_ - start);
    if (name_.empty())
        fail("a tag without a name");
    if (end) {
        while (pos_ < text_.size() && space(text_[pos_]))
            ++pos_;
        if (pos_ >= text_.size() || text_[pos_] != '>')
            fail("a malformed end tag");
        ++pos_;
        if (--depth_ < 0)
            fail("an end tag without its start");
        return Event::End;
    }
    for (;;) {
        while (pos_ < text_.size() && space(text_[pos_]))
            ++pos_;
        if (pos_ >= text_.size())
            fail("an unterminated tag");
        if (text_[pos_] == '>') {
            ++pos_;
            ++depth_;
            return Event::Start;
        }
        if (text_[pos_] == '/') {
            if (pos_ + 1 >= text_.size() || text_[pos_ + 1] != '>')
                fail("a malformed empty-element tag");
            pos_ += 2;
            ++depth_;
            pending_end_ = true;
            return Event::Start;
        }
        const std::size_t a = pos_;
        while (pos_ < text_.size() && !name_end(text_[pos_]))
            ++pos_;
        const std::string_view attribute = text_.substr(a, pos_ - a);
        while (pos_ < text_.size() && space(text_[pos_]))
            ++pos_;
        if (attribute.empty() || pos_ >= text_.size() || text_[pos_] != '=')
            fail("an attribute without a value");
        ++pos_;
        while (pos_ < text_.size() && space(text_[pos_]))
            ++pos_;
        if (pos_ >= text_.size() || (text_[pos_] != '"' && text_[pos_] != '\''))
            fail("an attribute value without quotes");
        const char quote = text_[pos_++];
        const std::size_t close = text_.find(quote, pos_);
        if (close == std::string_view::npos)
            fail("an unterminated attribute value");
        attributes_.emplace_back(attribute, text_.substr(pos_, close - pos_));
        pos_ = close + 1;
    }
}

std::optional<std::string_view> XmlReader::attribute(std::string_view local) const {
    for (const auto& [n, v] : attributes_)
        if (local_name(n) == local)
            return v;
    return std::nullopt;
}

std::string_view XmlReader::local_name(std::string_view name) {
    const std::size_t colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

std::string XmlReader::decode(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '&') {
            out.push_back(raw[i]);
            continue;
        }
        const std::size_t semi = raw.find(';', i);
        if (semi == std::string_view::npos)
            throw XmlError("XML: an unterminated reference in \"" + std::string(raw) + "\"");
        const std::string_view ref = raw.substr(i + 1, semi - i - 1);
        if (ref == "lt")
            out.push_back('<');
        else if (ref == "gt")
            out.push_back('>');
        else if (ref == "amp")
            out.push_back('&');
        else if (ref == "apos")
            out.push_back('\'');
        else if (ref == "quot")
            out.push_back('"');
        else if (ref.size() > 1 && ref[0] == '#') {
            const bool hex = ref[1] == 'x';
            const std::string_view digits = ref.substr(hex ? 2 : 1);
            std::uint32_t c = 0;
            const auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), c, hex ? 16 : 10);
            if (ec != std::errc() || end != digits.data() + digits.size() || c > 0x10FFFF || digits.empty())
                throw XmlError("XML: a bad character reference &" + std::string(ref) + ";");
            put_utf8(out, c);
        } else {
            throw XmlError("XML: an unknown entity &" + std::string(ref) + ";");
        }
        i = semi;
    }
    return out;
}

} // namespace tisma
