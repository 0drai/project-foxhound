#include "Taint.h"

#include <algorithm>
#include <iostream>

TaintOperation::TaintOperation(const char* name, std::initializer_list<std::u16string> args) : name_(name), arguments_(args) { }

TaintOperation::TaintOperation(const char* name, std::vector<std::u16string> args) : name_(name), arguments_(args) { }

TaintOperation::TaintOperation(const char* name) : name_(name), arguments_() { }

TaintOperation::TaintOperation(TaintOperation&& other) : name_(std::move(other.name_)), arguments_(std::move(other.arguments_)) { }

TaintOperation& TaintOperation::operator=(TaintOperation&& other)
{
    name_ = std::move(other.name_);
    arguments_ = std::move(other.arguments_);
    return *this;
}

TaintNode::TaintNode(TaintNode* parent, TaintOperation operation) : parent_(parent), refcount_(1), operation_(operation)
{
    if (parent_)
        parent_->addref();
}

TaintNode::TaintNode(TaintOperation operation) : parent_(nullptr), refcount_(1), operation_(operation) { }

void TaintNode::addref()
{
    if (refcount_ == 0xffffffff)
        MOZ_CRASH("TaintNode refcount overflow");

    refcount_++;
}

void TaintNode::release()
{
    MOZ_ASSERT(refcount_ > 0);

    refcount_--;
    if (refcount_ == 0)
        delete this;
}

TaintNode::~TaintNode()
{
    if (parent_)
        parent_->release();
}


TaintFlow::Iterator::Iterator(TaintNode* head) : current_(head) { }
TaintFlow::Iterator::Iterator() : current_(nullptr) { }
TaintFlow::Iterator::Iterator(const Iterator& other) : current_(other.current_) { }

TaintFlow::Iterator& TaintFlow::Iterator::operator++()
{
    current_ = current_->parent();
    return *this;
}

TaintNode& TaintFlow::Iterator::operator*() const
{
    return *current_;
}

bool TaintFlow::Iterator::operator==(const Iterator& other) const
{
    return current_ == other.current_;
}

bool TaintFlow::Iterator::operator!=(const Iterator& other) const
{
    return current_ != other.current_;
}

TaintFlow::TaintFlow() : head_(nullptr) { }

TaintFlow::TaintFlow(TaintNode* head) : head_(head) { }

TaintFlow::TaintFlow(TaintSource source) : head_(new TaintNode(source)) { }

TaintFlow::TaintFlow(const TaintFlow& other) : head_(other.head_)
{
    head_->addref();
}

TaintFlow::TaintFlow(TaintFlow&& other) : head_(other.head_)
{
    other.head_ = nullptr;
}


TaintFlow::~TaintFlow()
{
    if (head_) {
        head_->release();
    }
}

TaintFlow& TaintFlow::operator=(const TaintFlow& other)
{
    other.head_->addref();

    if (head_) {
        head_->release();
    }

    head_ = other.head_;

    return *this;
}

const TaintSource& TaintFlow::source() const
{
    TaintNode* source = head_;
    while (source->parent() != nullptr)
        source = source->parent();

    return source->operation();
}

TaintFlow& TaintFlow::extend(TaintOperation operation)
{
    TaintNode* newhead = new TaintNode(head_, operation);
    head_->release();
    head_ = newhead;
    return *this;
}

TaintFlow::iterator TaintFlow::begin() const
{
    return Iterator(head_);
}

TaintFlow::iterator TaintFlow::end() const
{
    return Iterator();
}

TaintFlow TaintFlow::extend(const TaintFlow& flow, TaintOperation operation)
{
    return TaintFlow(new TaintNode(flow.head_, operation));
}


TaintRange::TaintRange() : begin_(0), end_(0), flow_() { }
TaintRange::TaintRange(uint32_t begin, uint32_t end, TaintFlow flow) : begin_(begin), end_(end), flow_(flow)
{
    MOZ_ASSERT(begin <= end);
}
TaintRange::TaintRange(const TaintRange& other) : begin_(other.begin_), end_(other.end_), flow_(other.flow_) { }

TaintRange& TaintRange::operator=(const TaintRange& other)
{
    begin_ = other.begin_, end_ = other.end_;
    flow_ = other.flow_;

    return *this;
}

void TaintRange::resize(uint32_t begin, uint32_t end)
{
    MOZ_ASSERT(begin <= end);

    begin_ = begin;
    end_ = end;
}

StringTaint::StringTaint() : ranges_(nullptr) { }

StringTaint::StringTaint(TaintRange range)
{
    ranges_ = new std::vector<TaintRange>;
    ranges_->push_back(range);
}

StringTaint::StringTaint(uint32_t begin, uint32_t end, TaintOperation operation)
{
    ranges_ = new std::vector<TaintRange>;
    TaintRange range(begin, end, TaintFlow(new TaintNode(operation)));
    ranges_->push_back(range);
}

StringTaint::StringTaint(TaintFlow taint, uint32_t length)
{
    ranges_ = new std::vector<TaintRange>;
    ranges_->push_back(TaintRange(0, length, taint));
}

StringTaint& StringTaint::operator=(const StringTaint& other)
{
    if (this == &other)
        return *this;

    delete ranges_;

    if (other.ranges_)
        ranges_ = new std::vector<TaintRange>(*other.ranges_);
    else
        ranges_ = nullptr;

    return *this;
}

StringTaint& StringTaint::operator=(StringTaint&& other)
{
    delete ranges_;

    ranges_ = other.ranges_;
    other.ranges_ = nullptr;

    return *this;
}

StringTaint::StringTaint(const StringTaint& other) : ranges_(nullptr)
{
    if (other.ranges_)
        ranges_ = new std::vector<TaintRange>(*other.ranges_);
}

StringTaint::StringTaint(StringTaint&& other)
{
    ranges_ = other.ranges_;
    other.ranges_ = nullptr;
}

StringTaint::~StringTaint()
{
    clear();
}

void StringTaint::clear()
{
    delete ranges_;
    ranges_ = nullptr;
}

void StringTaint::clearBetween(uint32_t begin, uint32_t end)
{
    MOZ_ASSERT(begin <= end);

    auto ranges = new std::vector<TaintRange>();
    for (auto& range : *this) {
        if (range.end() <= begin || range.begin() >= end) {
            ranges->emplace_back(range.begin(), range.end(), range.flow());
        } else {
            if (range.begin() < begin)
                ranges->emplace_back(range.begin(), begin, range.flow());
            if (range.end() > end)
                ranges->emplace_back(end, range.end(), range.flow());
        }
    }

    assign(ranges);
}

void StringTaint::shift(uint32_t index, int amount)
{
    MOZ_ASSERT(index + amount >= 0);        // amount can be negative

    auto ranges = new std::vector<TaintRange>();
    StringTaint newtaint;
    for (auto& range : *this) {
        if (range.begin() >= index) {
            MOZ_ASSERT_IF(ranges_, range.begin() + amount >= ranges_->back().end());
            ranges->emplace_back(range.begin() + amount, range.end() + amount, range.flow());
        } else if (range.end() > index) {
            MOZ_ASSERT(amount >= 0);
            ranges->emplace_back(range.begin(), index, range.flow());
            ranges->emplace_back(index + amount, range.end() + amount, range.flow());
        } else {
            ranges->emplace_back(range.begin(), range.end(), range.flow());
        }
    }

    assign(ranges);
}

void StringTaint::insert(uint32_t index, const StringTaint& taint)
{
    auto ranges = new std::vector<TaintRange>();
    auto it = begin();

    while (it != end() && it->begin() < index) {
        auto& range = *it;
        MOZ_ASSERT(range.end() <= index);
        ranges->emplace_back(range.begin(), range.end(), range.flow());
        it++;
    }

    uint32_t last = index;
    for (auto& range : taint) {
        ranges->emplace_back(range.begin() + index, range.end() + index, range.flow());
        last = range.end() + index;
    }

    while (it != end()) {
        auto& range = *it;
        MOZ_ASSERT(range.begin() >= last);
        ranges->emplace_back(range.begin(), range.end(), range.flow());
        it++;
    }

    assign(ranges);
}

const TaintFlow* StringTaint::at(uint32_t index) const
{
    // TODO make this a binary search
    for (auto& range : *this) {
        if (range.begin() <= index && range.end() > index)
            return &range.flow();
    }
    return nullptr;
}

void StringTaint::set(uint32_t index, const TaintFlow& flow)
{
    // Common case: append a single character to a string.
    if (!ranges_ || index >= ranges_->back().end()) {
        append(TaintRange(index, index+1, flow));
    } else {
        clearAt(index);
        insert(index, StringTaint(TaintRange(index, index+1, flow)));
    }
}

StringTaint StringTaint::subtaint(uint32_t begin, uint32_t end) const
{
    MOZ_ASSERT(begin <= end);

    StringTaint newtaint;
    for (auto& range : *this) {
        if (range.begin() < end && range.end() > begin)
            newtaint.append(TaintRange(std::max(range.begin(), begin) - begin, std::min(range.end(), end) - begin, range.flow()));
    }

    return newtaint;
}

StringTaint& StringTaint::extend(TaintOperation operation)
{
    for (auto& range : *this)
        range.flow().extend(operation);

    return *this;
}

StringTaint& StringTaint::append(TaintRange range)
{
    MOZ_ASSERT_IF(ranges_, ranges_->back().end() <= range.begin());

    if (!ranges_)
        ranges_ = new std::vector<TaintRange>;

    // See if we can merge the two taint ranges.
    if (ranges_->size() > 0) {
        TaintRange& last = ranges_->back();
        if (last.end() == range.begin() && last.flow() == range.flow()) {
            last.resize(last.begin(), range.end());
            return *this;
        }
    }

    ranges_->push_back(range);

    return *this;
}

StringTaint& StringTaint::concat(const StringTaint& other, uint32_t offset)
{
    MOZ_ASSERT_IF(ranges_, ranges_->back().end() <= offset);

    for (auto& range : other)
        append(TaintRange(range.begin() + offset, range.end() + offset, range.flow()));

    return *this;
}

// Slight hack, see below.
static std::vector<TaintRange> empty_taint_range_vector;

StringTaint::iterator StringTaint::begin()
{
    // We still need to return an iterator even if there are no ranges stored in this instance.
    // In that case we don't have a std::vector though. Solution: use a static std::vector.
    if (!ranges_)
        return empty_taint_range_vector.begin();
    return ranges_->begin();
}

StringTaint::iterator StringTaint::end()
{
    if (!ranges_)
        return empty_taint_range_vector.end();
    return ranges_->end();
}

StringTaint::const_iterator StringTaint::begin() const
{
    if (!ranges_)
        return empty_taint_range_vector.begin();
    return ranges_->begin();
}

StringTaint::const_iterator StringTaint::end() const
{
    if (!ranges_)
        return empty_taint_range_vector.end();
    return ranges_->end();
}

StringTaint StringTaint::concat(const StringTaint& left, uint32_t leftlen, const StringTaint& right)
{
    StringTaint newtaint = left;
    return newtaint.concat(right, leftlen);
}

StringTaint StringTaint::substr(const StringTaint& taint, uint32_t begin, uint32_t end)
{
    return taint.subtaint(begin, end);
}

StringTaint StringTaint::extend(const StringTaint& taint, const TaintOperation& operation)
{
    StringTaint newtaint;
    for (auto& range : taint)
        newtaint.append(TaintRange(range.begin(), range.end(), TaintFlow::extend(range.flow(), operation)));

    return newtaint;
}

void StringTaint::assign(std::vector<TaintRange>* ranges)
{
    delete ranges_;
    if (ranges->size() > 0) {
        ranges_ = ranges;
    } else {
        ranges_ = nullptr;
        delete ranges;
    }
}


// Simple parser for a JSON like representation of taint information.
//
// Example:
//      [{begin: 10, end: 20, source: 'src1'}, {begin: 80, end: 90, source: 'src2'}]
//
// The ParseXXX methods always adjust |i| to point to the character directly after the end of the parsed object.

std::string ParseString(const std::string& str, size_t& i, size_t length, bool& valid)
{
    std::cout << "ParseKeyValuePair, i = " << i << std::endl;

    char c = str[i];

    // TODO support \' and \"
    size_t pos = str.find(c, i+1);
    if (pos == std::string::npos) {
        std::cout << "Errr: unterminated string literal" << std::endl;
        valid = false;
        return "";
    }

    valid = true;
    std::string res = str.substr(i + 1, pos - i - 1);
    i = pos + 1;
    return res;
}

std::pair<std::string, std::string> ParseKeyValuePair(const std::string& str, size_t& i, size_t length, bool& valid)
{
    std::cout << "ParseKeyValuePair, i = " << i << std::endl;

    std::string key, value;

    bool expecting_value = false, parsing_value = false;

    while (i < length) {
        char c = str[i];
        if (isalnum(c)) {
            if (expecting_value)
                parsing_value = true;
            if (!parsing_value)
                key.push_back(c);
            else
                value.push_back(c);
        } else if (c == ':') {
            if (expecting_value == true)
                break;
            expecting_value = true;
        } else if (c == '\'' || c == '"') {
            if (!expecting_value)
                break;
            parsing_value = true;
            value = ParseString(str, i, length, valid);
            break;
        } else if (parsing_value) {
            // done
            break;
        }
        i++;
    }

    if (!parsing_value) {
        std::cout << "Error: invalid key,value pair" << std::endl;
        valid = false;
    }

    valid = true;
    std::cout << "  Key: " << key << ", value: " << value << std::endl;
    return std::make_pair(key, value);
}

TaintRange ParseRange(const std::string& str, size_t& i, size_t length, bool& valid)
{
    std::cout << "ParseRange, i = " << i << std::endl;

    i++;

    size_t begin, end;
    std::string source;

    bool have_begin = false, have_end = false, have_source = false;

    while (i < length) {
        if (isalnum(str[i])) {
            std::pair<std::string, std::string> kv = ParseKeyValuePair(str, i, length, valid);
            if (!valid) {
                break;
            } else if (kv.first == "begin") {
                have_begin = true;
                begin = strtol(kv.second.c_str(), nullptr, 10);
            } else if (kv.first == "end") {
                have_end = true;
                end = strtol(kv.second.c_str(), nullptr, 10);
            } else if (kv.first == "source") {
                have_source = true;
                source = kv.second;
            } else {
                std::cout << "Warning: unknown key '" << kv.first << "'" << std::endl;
            }
        } else if (str[i] == '}') {
            i++;
            break;
        } else {
            i++;
        }
    }

    if (!valid || !have_begin || !have_end || !have_source) {
        std::cout << "Error: invalid taint range" << std::endl;
        valid = false;
        return TaintRange(0, 0, TaintSource(""));
    }

    valid = true;
    std::cout << "  ParseTaintRange done: " << begin << " - " << end << " : " << source << std::endl;
    return TaintRange(begin, end, TaintSource(source.c_str()));
}

StringTaint ParseTaint(const std::string& str)
{
    std::cout << "ParseTaint: " << str << std::endl;
    if (str.length() <= 2 || str.front() != '[' || str.back() != ']') {
        std::cout << "Error: malformed taint information" << std::endl;
        return EmptyTaint;
    }

    StringTaint taint;

    size_t i = 1, last_end = 0;
    size_t end = str.length() - 1;
    while (i < end) {
        if (str[i] == '{') {
            bool valid = false;
            TaintRange range = ParseRange(str, i, end, valid);
            if (!valid) {
                std::cout << "Error: malformed taint range" << std::endl;
                return EmptyTaint;
            }
            if (range.begin() < last_end) {
                std::cout << "Error: Invalid range, doesn't start after previous region" << std::endl;
                return EmptyTaint;
            }
            taint.append(range);
            last_end = range.end();
        }

        i++;
    }

    std::cout << "Done parsing taint. Result: " << std::endl;
    PrintTaint(taint);

    return taint;
}

void PrintTaint(const StringTaint& taint)
{
    for (auto& range : taint)
        std::cout << "    " << range.begin() << " - " << range.end() << " : " << range.flow().source().name() << std::endl;
}
