#pragma once

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>	
#include <type_traits>
#include <vector>
#include <cassert>


namespace WDBReader {
    class WDBReaderException : public std::logic_error {
    public:
        WDBReaderException(const std::string& what_arg, int code = 0) 
            : std::logic_error(what_arg), _code(code, std::generic_category()) {}
        WDBReaderException(const char* what_arg, int code = 0) 
            : std::logic_error(what_arg), _code(code, std::generic_category()) {}

        const std::error_code& getErrorCode() const {
            return _code;
        }

    private:
        std::error_code _code;
    };


	inline std::vector<std::string> split_string(const std::string& input, const std::string& separator) {
		std::vector<std::string> parts;

		if (input.size() > 0) {
			size_t start = 0;
			auto end = input.find(separator);
			while (end != input.npos) {
				parts.push_back(input.substr(start, end - start));
				start = end + separator.length();
				end = input.find(separator, start );
				//assert(start != end);
			}

			parts.push_back(input.substr(start));
		}

		return parts;
	}

	inline std::string trim(std::string s, const char* t = " \t\n\r\f\v")
	{
		s.erase(0, s.find_first_not_of(t));
		s.erase(s.find_last_not_of(t) + 1);
		return s;
	}

    struct GameVersion
    {
    public:
        uint16_t expansion;
        uint16_t major;
        uint16_t minor;
        uint32_t build;

        constexpr GameVersion(uint16_t a = 0, uint16_t b = 0, uint16_t c = 0, uint32_t d = 0) 
        : expansion(a), major(b), minor(c), build(d)
        {
        }

        GameVersion(const std::string& str) 
        : expansion(0), major(0), minor(0), build(0)
        {
            auto extract = [](const std::string& in, size_t start, auto& out, bool needs_end = true) -> size_t
            {
                size_t end;

                if (needs_end)
                {
                    end = in.find_first_of('.', start);
                    if (end == in.npos)
                    {
                        throw std::runtime_error("Build string is invalid.");
                    }
                }
                else
                {
                    end = in.npos;
                }

                auto temp_str = in.substr(start, end - start);
                std::from_chars(temp_str.data(), temp_str.data() + temp_str.size(), out);

                return end;
            };

            const auto first_pos = extract(str, 0, expansion);
            const auto second_pos = extract(str, first_pos + 1, major);
            const auto third_pos = extract(str, second_pos + 1, minor);
            extract(str, third_pos + 1, build, false);
        }

        inline static std::optional<GameVersion> fromString(const std::string& str)
        {
            try
            {
                return GameVersion(str);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        constexpr auto operator<=>(const GameVersion&) const = default;

        operator std::string() const {
            return toString();
        }

        inline std::string toString() const {
            return std::format("{}.{}.{}.{}", expansion, major, minor, build);
        }
    };

    template<std::invocable T>
    class ScopeGuard final {
        public:
        ScopeGuard(T&& callback) noexcept : _callback(std::move(callback)) {}
        ScopeGuard() = delete;
        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard(ScopeGuard&&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
        ScopeGuard& operator=(ScopeGuard&&) = delete;

        ~ScopeGuard() noexcept {
            _callback();
        }

        private:
        T _callback;
    };

    // wrapper around std::unique_ptr<T[]> with debug bounds checking.
    template<typename T>
    class DynArray final {
    public:
        using pointer = std::unique_ptr<T[]>::pointer;

        DynArray() = default;
        DynArray(size_t size) : _data(std::make_unique_for_overwrite<T[]>(size))
        {
#if _DEBUG
            _view = std::span<T>(_data.get(), size);
#endif
        }


        inline T& operator[](size_t index) {
#ifdef _DEBUG
            assert(index < _view.size());
#endif
            return _data[index];
        }

        inline pointer get() {
            return _data.get();
        }

    protected:
        std::unique_ptr<T[]> _data;

#ifdef _DEBUG
        std::span<T> _view;
#endif

    };
}