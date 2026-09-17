#include <deque>
#include <unordered_map>
#include <string>
#include <utility>

class HighPerformanceCache {
public:
    HighPerformanceCache(size_t capacity = 1000) : max_size(capacity) {
        data.reserve(max_size);
    }

    void insert(const std::string& key, const std::string& value) {
        auto it = key_to_idx.find(key);
        if (it != key_to_idx.end()) {
            data[it->second] = value;
        } else {
            if (data.size() >= max_size) {
                remove_oldest();
            }
            data.emplace_back(value);
            key_to_idx[key] = data.size() - 1;
        }
    }

    void remove(const std::string& key) {
        auto it = key_to_idx.find(key);
        if (it != key_to_idx.end()) {
            size_t idx = it->second;
            if (idx != data.size() - 1) {
                data[idx] = std::move(data.back());
                key_to_idx[data.back()] = idx;
            }
            data.pop_back();
            key_to_idx.erase(it);
        }
    }


private:
    void remove_oldest() {
        if (!data.empty()) {
            for (auto it = key_to_idx.begin(); it != key_to_idx.end(); ++it) {
                if (it->second == 0) {
                    key_to_idx.erase(it);
                    break;
                }
            }
            data.pop_front();
            for (auto& pair : key_to_idx) {
                if (pair.second > 0) --pair.second;
            }
        }
    }
    std::deque<std::string> data;
    std::unordered_map<std::string, size_t> key_to_idx;
    size_t max_size;
};
