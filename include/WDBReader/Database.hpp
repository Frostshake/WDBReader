#pragma once

#include "Filesystem.hpp"
#include "Database/Schema.hpp"
#include "Database/Formats.hpp"
#include <cstdint>
#include <type_traits>

namespace WDBReader::Database {

    template<TRecord R>
	class DataSource {
	public:
        virtual ~DataSource() = default;

        struct iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;

            using value_type = R;
            using pointer_type = R*;
            using reference_type = R&;

            iterator(const DataSource* owner, size_t index, R&& record) : 
                _owner(owner), _index(index), _loaded_index(-1), _val(std::move(record))
            {}

            iterator(const DataSource* owner, size_t index) :
                _owner(owner), _index(index), _loaded_index(-1)
            {}

            reference_type operator*()
            {
                set_val();
                return _val;
            }

            pointer_type operator->()
            {
                set_val();
                return &_val;
            }

            // Prefix increment
            iterator& operator++()
            {
                _index++;
                return *this;
            }

            // Postfix increment
            iterator operator++(int)
            {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const iterator& a, const iterator& b) { 
                assert(a._owner == b._owner);
                return a._index == b._index; 
            };
            friend bool operator!=(const iterator& a, const iterator& b) { 
                assert(a._owner == b._owner);
                return a._index != b._index; 
            };

        private:

            inline void set_val() {
                if (_index != _loaded_index) {
                    _val = std::move((*_owner)[_index]);
                    _loaded_index = _index;
                }
            }

            R _val;
            size_t _index;
            size_t _loaded_index;
            const DataSource* _owner;
        };

		virtual size_t size() const = 0;
		virtual R operator[](uint32_t index) const = 0;
        virtual Signature signature() const = 0;
        
        iterator begin() const {
            return cbegin();
        }

        iterator end() const {
            return cend();
        }

        iterator cbegin() const {
            if (this->size() > 0) {
                return iterator(this, 0, std::move((*this)[0]));
            }

            return cend();
        }

        iterator cend() const {
            return iterator(this, this->size());
        }
	};

	template<typename T, typename R, typename FS>
	concept TDataSource = requires(T t) {
        std::is_base_of_v<DataSource<R>, T>;
		TRecord<R>;
		Filesystem::TFileSource<FS>;
		{ t.open(std::unique_ptr<FS>()) };
		{ t.load() };
		{ t.size() } -> std::same_as<size_t>;
        { t.signature() } -> std::same_as<Signature>;
        { t[uint32_t()] } -> std::same_as<R>;
	};

}