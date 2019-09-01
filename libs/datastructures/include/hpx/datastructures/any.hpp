/*=============================================================================
    Copyright (c) 2013 Shuangyang Yang
    Copyright (c) 2007-2013 Hartmut Kaiser
    Copyright (c) Christopher Diggins 2005
    Copyright (c) Pablo Aguilar 2005
    Copyright (c) Kevlin Henney 2001

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    The class hpx::util::any is built based on boost::spirit::hold_any class.
    It adds support for HPX serialization, move assignment, == operator.
==============================================================================*/

#ifndef HPX_UTIL_ANY_HPP
#define HPX_UTIL_ANY_HPP

#include <hpx/config.hpp>
#include <hpx/assertion.hpp>
#include <hpx/traits/supports_streaming_with_any.hpp>

#include <algorithm>
#include <cstddef>
#include <iosfwd>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
#if defined(HPX_MSVC) && HPX_MSVC >= 1400
#pragma warning(push)
#pragma warning(disable : 4100)    // 'x': unreferenced formal parameter
#pragma warning(disable : 4127)    // conditional expression is constant
#endif

////////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace util {

    ////////////////////////////////////////////////////////////////////////////
    struct bad_any_cast : std::bad_cast
    {
        bad_any_cast(std::type_info const& src, std::type_info const& dest)
          : from(src.name())
          , to(dest.name())
        {
        }

        const char* what() const noexcept override
        {
            return "bad any cast";
        }

        const char* from;
        const char* to;
    };

    namespace detail { namespace any {

        ////////////////////////////////////////////////////////////////////////
        template <typename T>
        struct get_table;

        // function pointer table
        template <typename IArch, typename OArch, typename Char>
        struct fxn_ptr_table;

        template <>
        struct fxn_ptr_table<void, void, void>
        {
            virtual ~fxn_ptr_table() = default;
            virtual fxn_ptr_table* get_ptr() = 0;

            std::type_info const& (*get_type)();
            void (*static_delete)(void**);
            void (*destruct)(void**);
            void (*clone)(void* const*, void**);
            void (*copy)(void* const*, void**);
            bool (*equal_to)(void* const*, void* const*);
        };

        template <typename Char>
        struct fxn_ptr_table<void, void, Char> : fxn_ptr_table<void, void, void>
        {
            virtual ~fxn_ptr_table() = default;
            fxn_ptr_table* get_ptr() override = 0;

            std::basic_istream<Char>& (*stream_in)(
                std::basic_istream<Char>&, void**);
            std::basic_ostream<Char>& (*stream_out)(
                std::basic_ostream<Char>&, void* const*);
        };

        ////////////////////////////////////////////////////////////////////////
        template <typename T, typename Small, typename Char,
            typename Enable =
                typename traits::supports_streaming_with_any<T>::type>
        struct streaming_base;

        // no streaming support
        template <typename T>
        struct streaming_base<T, std::true_type, void, std::true_type>
        {
        };

        template <typename T>
        struct streaming_base<T, std::true_type, void, std::false_type>
        {
        };
        template <typename T>
        struct streaming_base<T, std::false_type, void, std::true_type>
        {
        };

        template <typename T>
        struct streaming_base<T, std::false_type, void, std::false_type>
        {
        };

        // streaming support is enabled
        template <typename T, typename Char>
        struct streaming_base<T, std::true_type, Char, std::true_type>
        {
            template <typename Char_>
            static std::basic_istream<Char_>& stream_in(
                std::basic_istream<Char_>& i, void** obj)
            {
                i >> *reinterpret_cast<T*>(obj);
                return i;
            }

            template <typename Char_>
            static std::basic_ostream<Char_>& stream_out(
                std::basic_ostream<Char_>& o, void* const* obj)
            {
                o << *reinterpret_cast<T const*>(obj);
                return o;
            }
        };

        template <typename T, typename Char>
        struct streaming_base<T, std::false_type, Char, std::true_type>
        {
            template <typename Char_>
            static std::basic_istream<Char_>& stream_in(
                std::basic_istream<Char_>& i, void** obj)
            {
                i >> **reinterpret_cast<T**>(obj);
                return i;
            }

            template <typename Char_>
            static std::basic_ostream<Char_>& stream_out(
                std::basic_ostream<Char>& o, void* const* obj)
            {
                o << **reinterpret_cast<T* const*>(obj);
                return o;
            }
        };

        template <typename T, typename Small, typename Char>
        struct streaming_base<T, Small, Char, std::false_type>
        {
            template <typename Char_>
            static std::basic_istream<Char_>& stream_in(
                std::basic_istream<Char_>& i, void** obj)
            {
                return i;
            }

            template <typename Char_>
            static std::basic_ostream<Char_>& stream_out(
                std::basic_ostream<Char_>& o, void* const* obj)
            {
                return o;
            }
        };

        ////////////////////////////////////////////////////////////////////////
        // static functions for small value-types
        template <typename Small>
        struct fxns;

        template <>
        struct fxns<std::true_type>
        {
            template <typename T, typename IArch, typename OArch,
                typename Char>
            struct type : public streaming_base<T, std::true_type, Char>
            {
                static fxn_ptr_table<IArch, OArch, Char>* get_ptr()
                {
                    return detail::any::get_table<T>::template get<IArch,
                        OArch, Char>();
                }

                static std::type_info const& get_type()
                {
                    return typeid(T);
                }
                static T& construct(void** f)
                {
                    new (f) T;
                    return *reinterpret_cast<T*>(f);
                }

                static T& get(void** f)
                {
                    return *reinterpret_cast<T*>(f);
                }

                static T const& get(void* const* f)
                {
                    return *reinterpret_cast<T const*>(f);
                }
                static void static_delete(void** x)
                {
                    reinterpret_cast<T*>(x)->~T();
                }
                static void destruct(void** x)
                {
                    reinterpret_cast<T*>(x)->~T();
                }
                static void clone(void* const* src, void** dest)
                {
                    new (dest) T(*reinterpret_cast<T const*>(src));
                }
                static void copy(void* const* src, void** dest)
                {
                    *reinterpret_cast<T*>(dest) =
                        *reinterpret_cast<T const*>(src);
                }
                static bool equal_to(void* const* x, void* const* y)
                {
                    return (get(x) == get(y));
                }
            };
        };

        // static functions for big value-types (bigger than a void*)
        template <>
        struct fxns<std::false_type>
        {
            template <typename T, typename IArch, typename OArch,
                typename Char>
            struct type : public streaming_base<T, std::false_type, Char>
            {
                static fxn_ptr_table<IArch, OArch, Char>* get_ptr()
                {
                    return detail::any::get_table<T>::template get<IArch,
                        OArch, Char>();
                }
                static std::type_info const& get_type()
                {
                    return typeid(T);
                }
                static T& construct(void** f)
                {
                    *f = new T;
                    return **reinterpret_cast<T**>(f);
                }
                static T& get(void** f)
                {
                    return **reinterpret_cast<T**>(f);
                }
                static T const& get(void* const* f)
                {
                    return **reinterpret_cast<T* const*>(f);
                }
                static void static_delete(void** x)
                {
                    // destruct and free memory
                    delete (*reinterpret_cast<T**>(x));
                }
                static void destruct(void** x)
                {
                    // destruct only, we'll reuse memory
                    (*reinterpret_cast<T**>(x))->~T();
                }
                static void clone(void* const* src, void** dest)
                {
                    *dest = new T(**reinterpret_cast<T* const*>(src));
                }
                static void copy(void* const* src, void** dest)
                {
                    **reinterpret_cast<T**>(dest) =
                        **reinterpret_cast<T* const*>(src);
                }
                static bool equal_to(void* const* x, void* const* y)
                {
                    return (get(x) == get(y));
                }
            };
        };

        ////////////////////////////////////////////////////////////////////////
        template <typename IArch, typename OArch, typename Vtable, typename Char>
        struct fxn_ptr;

        template <typename Vtable>
        struct fxn_ptr<void, void, Vtable, void>
          : fxn_ptr_table<void, void, void>
        {
            using base_type = fxn_ptr_table<void, void, void>;

            fxn_ptr()
            {
                base_type::get_type = Vtable::get_type;
                base_type::static_delete = Vtable::static_delete;
                base_type::destruct = Vtable::destruct;
                base_type::clone = Vtable::clone;
                base_type::copy = Vtable::copy;
                base_type::equal_to = Vtable::equal_to;
            }

            base_type* get_ptr() override
            {
                return Vtable::get_ptr();
            }
        };

        template <typename Vtable, typename Char>
        struct fxn_ptr<void, void, Vtable, Char>
          : fxn_ptr_table<void, void, Char>
        {
            using base_type = fxn_ptr_table<void, void, Char>;

            fxn_ptr()
            {
                base_type::get_type = Vtable::get_type;
                base_type::static_delete = Vtable::static_delete;
                base_type::destruct = Vtable::destruct;
                base_type::clone = Vtable::clone;
                base_type::copy = Vtable::copy;
                base_type::equal_to = Vtable::equal_to;
                base_type::stream_in = Vtable::stream_in;
                base_type::stream_out = Vtable::stream_out;
            }

            base_type* get_ptr() override
            {
                return Vtable::get_ptr();
            }
        };

        ////////////////////////////////////////////////////////////////////////
        template <typename T>
        struct get_table
        {
            using is_small =
                std::integral_constant<bool, (sizeof(T) <= sizeof(void*))>;

            template <typename IArch, typename OArch, typename Char>
            static fxn_ptr_table<IArch, OArch, Char>* get()
            {
                using fxn_type = typename fxns<is_small>::template type<T,
                    IArch, OArch, Char>;

                using fxn_ptr_type = fxn_ptr<IArch, OArch, fxn_type, Char>;

                static fxn_ptr_type static_table;

                return &static_table;
            }
        };

        ////////////////////////////////////////////////////////////////////////
        struct empty
        {
            bool operator==(empty const&) const
            {
                return false;    // undefined
            }
            bool operator!=(empty const&) const
            {
                return false;    // undefined
            }
        };

        template <typename Char>
        inline std::basic_istream<Char>& operator>>(
            std::basic_istream<Char>& i, empty&)
        {
            // If this assertion fires you tried to insert from a std istream
            // into an empty any instance. This simply can't work, because
            // there is no way to figure out what type to extract from the
            // stream.
            // The only way to make this work is to assign an arbitrary
            // value of the required type to the any instance you want to
            // stream to. This assignment has to be executed before the actual
            // call to the operator>>().
            HPX_ASSERT(false &&
                "Tried to insert from a std istream into an empty "
                "any instance");
            return i;
        }

        template <typename Char>
        inline std::basic_ostream<Char>& operator<<(
            std::basic_ostream<Char>& o, empty const&)
        {
            return o;
        }
    }}    // namespace detail::any
}}        // namespace hpx::util

namespace hpx { namespace util {

    ///////////////////////////////////////////////////////////////////////////
    template <typename IArch, typename OArch, typename Char = char>
    class basic_any;

    // specialization for any without streaming and without serialization
    template <>
    class basic_any<void, void, void>
    {
    public:
        // constructors
        basic_any() noexcept
            : table(detail::any::get_table<detail::any::empty>::
                    template get<void, void, void>())
            , object(nullptr)
        {
        }

        basic_any(basic_any const& x)
            : table(detail::any::get_table<detail::any::empty>::
                    template get<void, void, void>())
            , object(nullptr)
        {
            assign(x);
        }

        template <typename T>
        explicit basic_any(T const& x)
            : table(detail::any::get_table<typename std::decay<T>::type>::
                    template get<void, void, void>())
            , object(nullptr)
        {
            using value_type = typename std::decay<T>::type;
            new_object(object, x,
                typename detail::any::get_table<value_type>::is_small());
        }

        // Move constructor
        basic_any(basic_any&& x) noexcept
            : table(x.table)
            , object(x.object)
        {
            x.object = nullptr;
            x.table = detail::any::get_table<
                detail::any::empty>::template get<void, void, void>();
        }

        // Perfect forwarding of T
        template <typename T>
        explicit basic_any(T&& x,
            typename std::enable_if<!std::is_same<basic_any,
                typename std::decay<T>::type>::value>::type* = nullptr)
            : table(detail::any::get_table<typename std::decay<T>::type>::
                    template get<void, void, void>())
            , object(nullptr)
        {
            using value_type = typename std::decay<T>::type;
            new_object(object, std::forward<T>(x),
                typename detail::any::get_table<value_type>::is_small());
        }

        ~basic_any()
        {
            table->static_delete(&object);
        }

    private:
        basic_any& assign(basic_any const& x)
        {
            if (&x != this)
            {
                // are we copying between the same type?
                if (table == x.table)
                {
                    // if so, we can avoid reallocation
                    table->copy(&x.object, &object);
                }
                else
                {
                    reset();
                    x.table->clone(&x.object, &object);
                    table = x.table;
                }
            }
            return *this;
        }

        template <typename T>
        static void new_object(void*& object, T&& x, std::true_type)
        {
            using value_type = typename std::decay<T>::type;
            new (&object) value_type(std::forward<T>(x));
        }

        template <typename T>
        static void new_object(void*& object, T&& x, std::false_type)
        {
            using value_type = typename std::decay<T>::type;
            object = new value_type(std::forward<T>(x));
        }

    public:
        // copy assignment operator
        basic_any& operator=(basic_any const& x)
        {
            basic_any(x).swap(*this);
            return *this;
        }

        // move assignment
        basic_any& operator=(basic_any&& rhs)
        {
            rhs.swap(*this);
            basic_any().swap(rhs);
            return *this;
        }

        // Perfect forwarding of T
        template <typename T>
        basic_any& operator=(T&& rhs)
        {
            basic_any(std::forward<T>(rhs)).swap(*this);
            return *this;
        }

        // equality operator
        friend bool operator==(basic_any const& x, basic_any const& y)
        {
            if (&x == &y)    // same object
            {
                return true;
            }

            if (x.table == y.table)    // same type
            {
                return x.table->equal_to(
                    &x.object, &y.object);    // equal value?
            }

            return false;
        }

        template <typename T>
        friend bool operator==(basic_any const& b, T const& x)
        {
            using value_type = typename std::decay<T>::type;

            if (b.type() == typeid(value_type))    // same type
            {
                return b.cast<value_type>() == x;
            }

            return false;
        }

        // inequality operator
        friend bool operator!=(basic_any const& x, basic_any const& y)
        {
            return !(x == y);
        }

        template <typename T>
        friend bool operator!=(basic_any const& b, T const& x)
        {
            return !(b == x);
        }

        // utility functions
        basic_any& swap(basic_any& x) noexcept
        {
            std::swap(table, x.table);
            std::swap(object, x.object);
            return *this;
        }

        std::type_info const& type() const
        {
            return table->get_type();
        }

        template <typename T>
        T const& cast() const
        {
            if (type() != typeid(T))
                throw bad_any_cast(type(), typeid(T));

            return hpx::util::detail::any::get_table<T>::is_small::value ?
                *reinterpret_cast<T const*>(&object) :
                *reinterpret_cast<T const*>(object);
        }

// implicit casting is disabled by default for compatibility with boost::any
#ifdef HPX_ANY_IMPLICIT_CASTING
        // automatic casting operator
        template <typename T>
        operator T const&() const
        {
            return cast<T>();
        }
#endif    // implicit casting

        bool empty() const noexcept
        {
            return type() == typeid(detail::any::empty);
        }

        void reset()
        {
            if (!empty())
            {
                table->static_delete(&object);
                table = detail::any::get_table<
                    detail::any::empty>::template get<void, void, void>();
                object = nullptr;
            }
        }

    private:    // types
        template <typename T, typename IArch_, typename OArch_, typename Char_>
        friend T* any_cast(basic_any<IArch_, OArch_, Char_>*) noexcept;

        // fields
        detail::any::fxn_ptr_table<void, void, void>* table;
        void* object;
    };

    ////////////////////////////////////////////////////////////////////////////
    // specialization for hpx::any supporting streaming
    template <typename Char>    // default is char
    class basic_any<void, void, Char>
    {
    public:
        // constructors
        basic_any() noexcept
            : table(detail::any::get_table<detail::any::empty>::template get<
                void, void, Char>())
            , object(nullptr)
        {
        }

        basic_any(basic_any const& x)
            : table(detail::any::get_table<detail::any::empty>::template get<
                void, void, Char>())
            , object(nullptr)
        {
            assign(x);
        }

        template <typename T>
        explicit basic_any(T const& x)
            : table(detail::any::get_table<typename std::decay<T>::type>::
                    template get<void, void, Char>())
            , object(nullptr)
        {
            using value_type = typename std::decay<T>::type;
            new_object(object, x,
                typename detail::any::get_table<value_type>::is_small());
        }

        // Move constructor
        basic_any(basic_any&& x) noexcept
            : table(x.table)
            , object(x.object)
        {
            x.object = nullptr;
            x.table = detail::any::get_table<
                detail::any::empty>::template get<void, void, Char>();
        }

        // Perfect forwarding of T
        template <typename T>
        explicit basic_any(T&& x,
            typename std::enable_if<!std::is_same<basic_any,
                typename std::decay<T>::type>::value>::type* = nullptr)
            : table(detail::any::get_table<typename std::decay<T>::type>::
                    template get<void, void, Char>())
            , object(nullptr)
        {
            using value_type = typename std::decay<T>::type;
            new_object(object, std::forward<T>(x),
                typename detail::any::get_table<value_type>::is_small());
        }

        ~basic_any()
        {
            table->static_delete(&object);
        }

    private:
        basic_any& assign(basic_any const& x)
        {
            if (&x != this)
            {
                // are we copying between the same type?
                if (table == x.table)
                {
                    // if so, we can avoid reallocation
                    table->copy(&x.object, &object);
                }
                else
                {
                    reset();
                    x.table->clone(&x.object, &object);
                    table = x.table;
                }
            }
            return *this;
        }

        template <typename T>
        static void new_object(void*& object, T&& x, std::true_type)
        {
            using value_type = typename std::decay<T>::type;
            new (&object) value_type(std::forward<T>(x));
        }

        template <typename T>
        static void new_object(void*& object, T&& x, std::false_type)
        {
            using value_type = typename std::decay<T>::type;
            object = new value_type(std::forward<T>(x));
        }

    public:
        // copy assignment operator
        basic_any& operator=(basic_any const& x)
        {
            basic_any(x).swap(*this);
            return *this;
        }

        // move assignment
        basic_any& operator=(basic_any&& rhs)
        {
            rhs.swap(*this);
            basic_any().swap(rhs);
            return *this;
        }

        // Perfect forwarding of T
        template <typename T>
        basic_any& operator=(T&& rhs)
        {
            basic_any(std::forward<T>(rhs)).swap(*this);
            return *this;
        }

        // equality operator
        friend bool operator==(basic_any const& x, basic_any const& y)
        {
            if (&x == &y)    // same object
            {
                return true;
            }

            if (x.table == y.table)    // same type
            {
                return x.table->equal_to(
                    &x.object, &y.object);    // equal value?
            }

            return false;
        }

        template <typename T>
        friend bool operator==(basic_any const& b, T const& x)
        {
            using value_type = typename std::decay<T>::type;

            if (b.type() == typeid(value_type))    // same type
            {
                return b.cast<value_type>() == x;
            }

            return false;
        }

        // inequality operator
        friend bool operator!=(basic_any const& x, basic_any const& y)
        {
            return !(x == y);
        }

        template <typename T>
        friend bool operator!=(basic_any const& b, T const& x)
        {
            return !(b == x);
        }

        // utility functions
        basic_any& swap(basic_any& x) noexcept
        {
            std::swap(table, x.table);
            std::swap(object, x.object);
            return *this;
        }

        std::type_info const& type() const
        {
            return table->get_type();
        }

        template <typename T>
        T const& cast() const
        {
            if (type() != typeid(T))
                throw bad_any_cast(type(), typeid(T));

            return hpx::util::detail::any::get_table<T>::is_small::value ?
                *reinterpret_cast<T const*>(&object) :
                *reinterpret_cast<T const*>(object);
        }

// implicit casting is disabled by default for compatibility with boost::any
#ifdef HPX_ANY_IMPLICIT_CASTING
        // automatic casting operator
        template <typename T>
        operator T const&() const
        {
            return cast<T>();
        }
#endif    // implicit casting

        bool empty() const noexcept
        {
            return type() == typeid(detail::any::empty);
        }

        void reset()
        {
            if (!empty())
            {
                table->static_delete(&object);
                table = detail::any::get_table<
                    detail::any::empty>::template get<void, void, Char>();
                object = nullptr;
            }
        }

        // These functions have been added in the assumption that the embedded
        // type has a corresponding operator defined, otherwise use the
        // specialization above.
        template <typename IArch_, typename OArch_, typename Char_>
        friend std::basic_istream<Char_>& operator>>(
            std::basic_istream<Char_>& i,
            basic_any<IArch_, OArch_, Char_>& obj);

        template <typename IArch_, typename OArch_, typename Char_>
        friend std::basic_ostream<Char_>& operator<<(
            std::basic_ostream<Char_>& o,
            basic_any<IArch_, OArch_, Char_> const& obj);

    private:    // types
        template <typename T, typename IArch_, typename OArch_, typename Char_>
        friend T* any_cast(basic_any<IArch_, OArch_, Char_>*) noexcept;

        // fields
        detail::any::fxn_ptr_table<void, void, Char>* table;
        void* object;
    };

    ///////////////////////////////////////////////////////////////////////////
    template <typename IArch_, typename OArch_, typename Char_>
    std::basic_istream<Char_>& operator>>(std::basic_istream<Char_>& i,
        basic_any<IArch_, OArch_, Char_>& obj)
    {
        return obj.table->stream_in(i, &obj.object);
    }

    template <typename IArch_, typename OArch_, typename Char_>
    std::basic_ostream<Char_>& operator<<(std::basic_ostream<Char_>& o,
        basic_any<IArch_, OArch_, Char_> const& obj)
    {
        return obj.table->stream_out(o, &obj.object);
    }

    ///////////////////////////////////////////////////////////////////////////
    template <typename IArch, typename OArch, typename Char>
    void swap(basic_any<IArch, OArch, Char>& lhs,
        basic_any<IArch, OArch, Char>& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // boost::any-like casting
    template <typename T, typename IArch, typename OArch, typename Char>
    inline T* any_cast(basic_any<IArch, OArch, Char>* operand) noexcept
    {
        if (operand && operand->type() == typeid(T))
        {
            return hpx::util::detail::any::get_table<T>::is_small::value ?
                reinterpret_cast<T*>(
                    reinterpret_cast<void*>(&operand->object)) :
                reinterpret_cast<T*>(
                    reinterpret_cast<void*>(operand->object));
        }
        return nullptr;
    }

    template <typename T, typename IArch, typename OArch, typename Char>
    inline T const* any_cast(
        basic_any<IArch, OArch, Char> const* operand) noexcept
    {
        return any_cast<T>(const_cast<basic_any<IArch, OArch, Char>*>(operand));
    }

    template <typename T, typename IArch, typename OArch, typename Char>
    T any_cast(basic_any<IArch, OArch, Char>& operand)
    {
        using nonref = typename std::remove_reference<T>::type;

        nonref* result = any_cast<nonref>(&operand);
        if (!result)
            throw bad_any_cast(operand.type(), typeid(T));
        return static_cast<T>(*result);
    }

    template <typename T, typename IArch, typename OArch, typename Char>
    T const& any_cast(basic_any<IArch, OArch, Char> const& operand)
    {
        using nonref = typename std::remove_reference<T>::type;

        return any_cast<nonref const&>(
            const_cast<basic_any<IArch, OArch, Char>&>(operand));
    }

    ////////////////////////////////////////////////////////////////////////////
    using any_nonser = basic_any<void, void, void>;

    using streamable_any_nonser = basic_any<void, void, char>;
    using streamable_wany_nonser = basic_any<void, void, wchar_t>;

}}    // namespace hpx::util

////////////////////////////////////////////////////////////////////////////////
#if defined(HPX_MSVC) && HPX_MSVC >= 1400
#pragma warning(pop)
#endif

#endif