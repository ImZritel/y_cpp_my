#pragma once
#include <vector>

template <typename It>
class Paginator {
public:
    explicit Paginator(It begin, It end, size_t page_size) {
        auto iit = begin;
        bool next = true;
        while (distance(begin, end) > 0 && next) {
            if (distance(begin, end) > page_size) {
                advance(iit, page_size);
                result_pages_.push_back(std::pair{ begin, iit });
                advance(begin, page_size);
            }
            else if (distance(begin, end) == page_size) {
                result_pages_.push_back(std::pair{ begin, end });
                next = false;
            }
            else if (distance(begin, end) < page_size) {
                result_pages_.push_back(std::pair{ begin, end });
                next = false;
            }
        }
    }

    auto begin() const {
        /*returns an iterator to the beginning */
        return result_pages_.begin();
    }
    auto end() const {
        /*returns an iterator to the end*/
        return result_pages_.end();
    }
    auto size() const {
        return distance(result_pages_.end(), result_pages_.begin());
    }
private:
    std::vector<std::pair<It, It>> result_pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
