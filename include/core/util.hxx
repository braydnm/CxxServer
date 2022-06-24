#pragma once

#include "core/properties.hxx"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>



namespace CxxServer::Core::Utils {
    inline std::string_view fastItoa(std::size_t val, char *buf, std::size_t buf_size) {
        std::size_t idx = buf_size;
        do {
            buf[--idx] = '0' + (val % 10);
            val /= 10;
        } while(val > 0);

        return {buf + idx, buf_size - idx};
    }

    class CacheView {
    public:
        CacheView(std::weak_ptr<std::string> str, std::size_t idx = 0, std::size_t size = 0) : _str(str), _idx(idx), _size(size) {}
        CacheView(CacheView &&) = default;
        CacheView &operator=(CacheView &&) = default;
        CacheView(CacheView &) = default;
        CacheView &operator=(CacheView &) = default;
        
        std::size_t &idx() noexcept { return _idx; }
        const std::size_t &idx() const noexcept { return _idx; }
        const std::size_t &size() const noexcept { return _size; }
        std::size_t &size() noexcept { return _size; }

        const char *data() const noexcept { 
            if (auto tmp = _str.lock())
                return tmp->c_str() + _idx;
            
            return nullptr;
        }

        constexpr CacheView &operator=(std::initializer_list<std::size_t> s) {
            // static_assert(s.size() == 2, "Expect idx & size arguments");
            _idx = *s.begin();
            _size = *(s.begin()+1);
            return *this;
        }

        operator std::string_view() const { 
            if (auto tmp = _str.lock())
                return std::string_view(tmp->c_str() + _idx, _size);
            
            return {NULL, 0};
        }

    private:
        std::weak_ptr<std::string> _str;
        std::size_t _idx;
        std::size_t _size;
    };

    struct CacheViewSparseMapHash {
        using transparent_key_equal = CacheViewSparseMapHash;
        using is_transparent = void;

        template<typename T>
        int compare(const CacheView &view1, const T &view2) const {
            const std::size_t _rlen = std::min(view1.size(), view2.size());
            int ret = std::char_traits<char>::compare(view1.data(), view2.data(), _rlen);
            if (ret == 0)
                ret = view1.size() < view2.size() ? -1 : (view1.size() == view2.size() ? 0 : 1);

            return ret;
        }

        std::size_t operator()(const std::string_view &view1) const {
            return std::hash<std::string_view>()(view1);
        }

        std::size_t operator()(const CacheView &view1) const { 
            return std::hash<std::string_view>()(view1);
        }

        bool operator()(const CacheView &view1, const CacheView &view2) const {
            return compare(view1, view2) == 0;
        }

        bool operator()(const CacheView &view1, const std::string_view &view2) const {
            return compare(view1, view2) == 0;
        }

        bool operator()(const std::string_view &view1, const CacheView &view2) const {
            return this->operator()(view2, view1);
        }
    };
}

template <>
struct std::hash<CxxServer::Core::Utils::CacheView> {
    typedef CxxServer::Core::Utils::CacheView argument_type;
    typedef std::size_t result_type;

    result_type operator() (const argument_type &val) const {
        return std::hash<std::string_view>()(val);
    }
};