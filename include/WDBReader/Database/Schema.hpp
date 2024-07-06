#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <stdexcept>	
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace WDBReader::Database
{

    using string_ref_t = uint32_t;
    using lang_string_ref_t = uint32_t;

    static_assert(sizeof(char) == sizeof(uint8_t));
    using string_data_t = std::unique_ptr<char[]>;
    using string_data_ref_t = const char *;

    static_assert(sizeof(string_data_t) == sizeof(string_data_ref_t));

    using runtime_value_t = std::variant<uint8_t, uint16_t, uint32_t, uint64_t, float, string_data_t>;
    using runtime_value_ref_t = std::variant<uint8_t, uint16_t, uint32_t, uint64_t, float, string_data_ref_t>;

    static_assert(sizeof(runtime_value_t) == sizeof(runtime_value_ref_t));

    enum class RecordEncryption : uint8_t {
        NONE,
        DECRYPTED,
        ENCRYPTED
    };

    template <typename T>
    concept TSchema = requires(T t) {
        { t.fields() };
        { t.fields().size() } -> std::same_as<size_t>;
        { t.elementCount() } -> std::same_as<size_t>;
    };

    template <typename T>
    concept TNamedSchema = requires(T t) {
        { TSchema<T> };
        { t.names() };
        { t.names().size() } -> std::same_as<size_t>;
    };

    template <typename T>
    concept TRecord = requires(T t) {
        { T::make(std::declval<T *>(), uint32_t(), uint32_t()) };
        { T::insertField(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t()) };
        { T::insertValue(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t(), uint8_t()) };
        { T::insertValue(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t(), uint16_t()) }; 
        { T::insertValue(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t(), uint32_t()) }; 
        { T::insertValue(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t(), uint64_t()) };
        { T::insertValue(std::declval<T*>(), uint32_t(), uint32_t(), ptrdiff_t(), float()) };
        { t.recordIndex };
        std::is_integral_v<decltype(t.recordIndex)>;
        { t.encryptionState };
        std::is_same_v<decltype(t.encryptionState), RecordEncryption>;
        { t.data };
    };

    struct Annotation final
    {
    public:
        constexpr Annotation(bool id = false, bool rel = false, bool inl = true) : isId(id), isRelation(rel), isInline(inl) {}

        bool isId;
        bool isRelation ;
        bool isInline ;

        inline constexpr Annotation& Id()
        {
            this->isId = true;
            return *this;
        }

        inline constexpr Annotation& Relation()
        {
            this->isRelation = true;
            return *this;
        }

        inline constexpr Annotation& NonInline()
        {
            this->isInline = false;
            return *this;
        }
    };

    constexpr bool operator==(const Annotation& a, const Annotation& b)
    {
        return a.isId == b.isId &&
               a.isRelation == b.isRelation &&
               a.isInline == b.isInline;
    }

    constexpr bool operator!=(const Annotation& a, const Annotation& b)
    {
        return !(a == b);
    }

    class Field final
    {
    public:
        enum class Type
        {
            INT,
            FLOAT,
            STRING,
            LANG_STRING,
        };

        const Type type;

        /// <summary>
        /// element size in bytes (per element)
        /// </summary>
        const uint8_t bytes;

        /// <summary>
        /// number of elements (arrays are size > 1)
        /// </summary>
        const uint8_t size;

        const Annotation annotation;

        inline constexpr bool isArray() const
        {
            return size > 1;
        }

        inline constexpr uint16_t totalBytes() const
        {
            return bytes * size;
        }

        inline constexpr static Field integer(uint8_t size, Annotation ann = Annotation())
        {
            return Field(Type::INT, size, 1, ann);
        }

        inline constexpr static Field integerArray(uint16_t totalBytes, uint8_t count, Annotation ann = Annotation())
        {
            return Field(Type::INT, totalBytes / count, count, ann);
        }

        inline constexpr static Field floatingPoint(uint8_t size, Annotation ann = Annotation())
        {
            return Field(Type::FLOAT, size, 1, ann);
        }

        inline constexpr static Field floatingPointArray(uint16_t totalBytes, uint8_t count, Annotation ann = Annotation())
        {
            return Field(Type::FLOAT, totalBytes / count, count, ann);
        }

        inline constexpr static Field string(uint8_t count = 1, Annotation ann = Annotation())
        {
            return Field(Type::STRING, sizeof(string_data_t), count, ann);
        }

        inline constexpr static Field langString(uint8_t count = 1, Annotation ann = Annotation())
        {
            return Field(Type::LANG_STRING, sizeof(string_data_t), count, ann);
        }

        ///*
        //	Helpers for int and float types.
        //*/
        template <typename T>
        inline constexpr static Field value(Annotation ann = Annotation())
            requires std::is_floating_point_v<T>
        {
            return Field::floatingPoint(sizeof(T), ann);
        }

        template <typename T>
        inline constexpr static Field value(Annotation ann = Annotation())
            requires std::is_integral_v<T>
        {
            return Field::integer(sizeof(T), ann);
        }

        template <typename T>
        inline constexpr static Field value(Annotation ann = Annotation())
            requires std::is_array_v<T> && std::is_floating_point_v<std::remove_extent_t<T>>
        {
            return Field::floatingPointArray(sizeof(T), std::extent<T>::value, ann);
        }

        template <typename T>
        inline constexpr static Field value(Annotation ann = Annotation())
            requires std::is_array_v<T> && std::is_integral_v<std::remove_extent_t<T>>
        {
            return Field::integerArray(sizeof(T), std::extent<T>::value, ann);
        }

    protected:
        constexpr Field(Type t, uint8_t b, uint8_t s = 1, Annotation ann = Annotation())
            : type(t), bytes(b), size(s), annotation(ann)
        {
        }
    };

    constexpr bool operator==(const Field& a, const Field& b)
    {
        return a.type == b.type &&
               a.size == b.size &&
               a.bytes == b.bytes &&
               a.annotation == b.annotation;
    }

    constexpr bool operator!=(const Field& a, const Field& b)
    {
        return !(a == b);
    }

    template <class... T>
    class Schema
    {
    public:
        using field_container_t = std::array<const Field, sizeof...(T)>;

        constexpr Schema(const T&&... init) : _fields{{init...}}
        {
        }

        constexpr field_container_t fields() const
        {
            return _fields;
        }

        constexpr size_t elementCount() const
        {
            return std::accumulate(_fields.cbegin(), _fields.cend(),
                (size_t)0,
                [](size_t sum, const Field& field) constexpr
                {
                    return sum + field.size;
                });
        }

    private:
        const field_container_t _fields;
    };

    class RuntimeSchema
    {
    public:
        using field_container_t = std::vector<Field>;
        using field_name_t = std::string;

        template <TRecord R>
        struct record_accessor
        {
        public:
            using record_value_t = std::span<const runtime_value_ref_t>;

            struct const_iterator
            {
            public:
                using iterator_category = std::forward_iterator_tag;
                using difference_type = std::ptrdiff_t;

                struct value_t
                {
                public:
                    const field_name_t* name;
                    const Field* field;
                    record_value_t value;
                };

                using value_type = const value_t;
                using pointer_type = const value_t*;
                using reference_type = const value_t&;

                const_iterator(const record_accessor<R>* accessor, size_t index) : _accessor(accessor), _index(index), _loaded_index(-1)
                {
                    assert(index <= _accessor->_schema.fields().size());
#ifdef _DEBUG
                    // load value before access so it can shown in IDE.
                    if (index < _accessor->_schema.fields().size())
                    {
                        set_val();
                    }
#endif
                }

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
                const_iterator& operator++()
                {
                    _index++;
                    return *this;
                }

                // Postfix increment
                const_iterator operator++(int)
                {
                    const_iterator tmp = *this;
                    ++(*this);
                    return tmp;
                }

                friend bool operator==(const const_iterator& a, const const_iterator& b) { return a._index == b._index; };
                friend bool operator!=(const const_iterator& a, const const_iterator& b) { return a._index != b._index; };

            private:
                inline void set_val()
                {
                    if (_index != _loaded_index)
                    {
                        const auto& name = _accessor->_schema._names[_index];
                        const auto& field = _accessor->_schema._fields[_index];
                        const auto& offset = _accessor->_schema._field_offsets[_index];

                        _val.name = &name;
                        _val.field = &field;
                        _val.value = {
                            reinterpret_cast<const runtime_value_ref_t*>(&_accessor->_record.data[offset]),
                            field.size
                        };

                        _loaded_index = _index;
                    }
                }

                const record_accessor<R>* _accessor;
                size_t _index;
                value_t _val;
                size_t _loaded_index;
            };

            inline constexpr record_accessor(const RuntimeSchema &schema, const R& record) : _schema(schema), _record(record)
            {
                assert(_record.data.size() == _schema.elementCount());
            }

            inline constexpr const_iterator begin() const
            {
                return cbegin();
            }

            inline constexpr const_iterator end() const
            {
                return cend();
            }

            inline constexpr const_iterator cbegin() const
            {
                return const_iterator(this, 0);
            }

            inline constexpr const_iterator cend() const
            {
                return const_iterator(this, _schema.fields().size());
            }

            inline constexpr record_value_t operator[](const field_name_t& name) const
            {
                const auto index = name_index(name);
                const auto &field = _schema._fields[index];
                const auto &offset = _schema._field_offsets[index];

                return {
                    reinterpret_cast<const runtime_value_ref_t*>(&_record.data[offset]),
                    field.size
                };
            }

        private:
            inline uint32_t name_index(const field_name_t& name) const
            {
                const auto it = std::find(_schema._names.cbegin(), _schema._names.cend(), name);

                if (it == _schema._names.cend())
                {
                    throw std::out_of_range("Name doesnt exist.");
                }

                return std::distance(_schema._names.cbegin(), it);
            }

            const R& _record;
            const RuntimeSchema& _schema;
        };

        RuntimeSchema(std::vector<Field>&& fields, std::vector<field_name_t>&& names) 
            : _fields(std::move(fields)), _names(std::move(names))
        {
            if (_names.size() != _fields.size())
            {
                throw std::logic_error("Fields size doesnt match names size.");
            }

            _element_count = 0;
            _field_offsets.reserve(_fields.size());
            uint32_t pos = 0;
            for (auto it = _fields.cbegin(); it != _fields.cend(); ++it)
            {
                _field_offsets.push_back(pos);
                pos += it->size;
                _element_count += it->size;
            }
        }
        RuntimeSchema(const RuntimeSchema&) = default;
        RuntimeSchema(RuntimeSchema&&) = default;
        RuntimeSchema& operator=(const RuntimeSchema&) = default;
        RuntimeSchema& operator=(RuntimeSchema&&) = default;

        const field_container_t& fields() const
        {
            return _fields;
        }

        const std::vector<field_name_t>& names() const
        {
            return _names;
        }

        template <TRecord R>
        inline const record_accessor<R> operator()(const R& record) const
        {
            return record_accessor<R>(*this, record);
        }

        constexpr size_t elementCount() const
        {
            return _element_count;
        }


    private:
        field_container_t _fields;
        std::vector<field_name_t> _names;
        std::vector<uint32_t> _field_offsets;
        size_t _element_count;
    };

    template <TSchema S1, TSchema S2>
    constexpr bool operator==(const S1& a, const S2& b)
    {

        if (!std::ranges::equal(a.fields(), b.fields()))
        {
            return false;
        }

        if constexpr (TNamedSchema<S1> && TNamedSchema<S2>)
        {
            if (!std::ranges::equal(a.names(), b.names()))
            {
                return false;
            }
        }

        return true;
    }

    template <TSchema S1, TSchema S2>
    constexpr bool operator!=(const S1& a, const S2& b)
    {
        return !(a == b);
    }

    template <typename R>
    struct FixedRecord
    {
    public:
        inline constexpr static void make(R* record, uint32_t element_count, uint32_t record_size)
        {
        }

        inline constexpr static void insertField(R* record, uint32_t field_index, uint32_t array_size, ptrdiff_t data_offset)
        {
        }

        template <typename T>
        inline constexpr static void insertValue(R* record,
                                                 uint32_t field_index, uint32_t array_index, ptrdiff_t dest_data_offset, T&& value)
        {
            using t_value = std::decay_t<T>;
            uint8_t* const field_offset = ((uint8_t*)&record->data) + dest_data_offset;
            *((t_value*)field_offset) = std::move(value);
        }
    };

    template <typename R>
    struct VariableRecord
    {
    public:
        inline constexpr static void make(R* record, uint32_t element_count, uint32_t record_size)
        {
            record->data.reserve(element_count);
        }

        inline constexpr static void insertField(R* record, uint32_t field_index, uint32_t array_size, ptrdiff_t data_offset)
        {
        }

        template <typename T>
        inline constexpr static void insertValue(R* record,
                                                 uint32_t field_index, uint32_t array_index, ptrdiff_t dest_data_offset, T&& value)
        {
            record->data.emplace_back(std::move(value));
        }
    };

    struct RuntimeRecord : public VariableRecord<RuntimeRecord>
    {
    public:
        std::vector<runtime_value_t> data;
        size_t recordIndex;
        RecordEncryption encryptionState;
    };

    template <typename T>
    inline void schemaFieldHandler(const Field &field, T handler)
    {
        switch (field.type)
        {
        case Field::Type::INT:
            switch (field.bytes)
            {
            case sizeof(uint32_t):
            {
                handler.operator()<uint32_t>();
            }
            break;

            case sizeof(uint8_t):
            {
                handler.operator()<uint8_t>();
            }
            break;

            case sizeof(uint16_t):
            {
                handler.operator()<uint16_t>();
            }
            break;

            case sizeof(uint64_t):
            {
                handler.operator()<uint64_t>();
            }
            break;
            default:
                throw std::logic_error("Handled integer size.");
                break;
            }
            break;
        case Field::Type::FLOAT:
        {
            assert(field.bytes == sizeof(float));
            handler.operator()<float>();
        }
        break;
        case Field::Type::LANG_STRING:
        case Field::Type::STRING:
        {
            handler.operator()<string_data_t>();
        }
        break;

        default:
            throw std::logic_error("Unhandled field type.");
            break;
        }
    }

}