// Copyright Eric Niebler 2014
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//
#ifndef RANGES_V3_RANGE_ADAPTOR_HPP
#define RANGES_V3_RANGE_ADAPTOR_HPP

#include <range/v3/range_fwd.hpp>
#include <range/v3/range_facade.hpp>
#include <range/v3/range_traits.hpp>

namespace ranges
{
    inline namespace v3
    {
        namespace detail
        {
            template<typename Iterable>
            using cursor_adaptor_t = decltype(range_core_access::begin_adaptor(std::declval<Iterable const &>()));

            template<typename Iterable>
            using sentinel_adaptor_t = decltype(range_core_access::end_adaptor(std::declval<Iterable const &>()));

            template<typename Iterator, bool IsIterator = ranges::Iterator<Iterator>()>
            struct is_single_pass
            {
                using type = Same<iterator_category_t<Iterator>, std::input_iterator_tag>;
            };

            template<typename Sentinel>
            struct is_single_pass<Sentinel, false>
            {
                using type = std::false_type;
            };

            // Give Iterable::iterator a simple interface for passing to Derived
            template<typename IteratorOrSentinel>
            struct basic_adaptor
            {
            private:
                template<typename Other>
                friend struct basic_adaptor;
                IteratorOrSentinel it_;
            public:
                using single_pass = typename is_single_pass<IteratorOrSentinel>::type;
                basic_adaptor() = default;
                constexpr basic_adaptor(IteratorOrSentinel it)
                  : it_(detail::move(it))
                {}
                template<typename Other,
                    CONCEPT_REQUIRES_(EqualityComparable<IteratorOrSentinel, Other>())>
                constexpr bool equal(basic_adaptor<Other> const &that) const
                {
                    return it_ == that.it_;
                }
                CONCEPT_REQUIRES(Iterator<IteratorOrSentinel>())
                void next()
                {
                    ++it_;
                }
                template<typename I = IteratorOrSentinel,
                    CONCEPT_REQUIRES_(Iterator<IteratorOrSentinel>())>
                auto current() const -> decltype(*std::declval<I const &>())
                {
                    return *it_;
                }
                CONCEPT_REQUIRES(BidirectionalIterator<IteratorOrSentinel>())
                void prev()
                {
                    --it_;
                }
                template<typename I = IteratorOrSentinel,
                    CONCEPT_REQUIRES_(RandomAccessIterator<IteratorOrSentinel>())>
                void advance(iterator_difference_t<I> n)
                {
                    it_ += n;
                }
                template<typename I = IteratorOrSentinel,
                    CONCEPT_REQUIRES_(RandomAccessIterator<IteratorOrSentinel>())>
                iterator_difference_t<I>
                distance_to(basic_adaptor const &that) const
                {
                    return that.it_ - it_;
                }
            };
        }

        template<typename Derived>
        using range_adaptor_t = typename Derived::range_adaptor_t;

        // TODO: or, if BaseIterable was created with range_facade, base_cursor_t
        // could simply be the underlying cursor type and base_sentinel_t the underlying
        // sentinel.
        template<typename Derived>
        using base_cursor_t =
            detail::basic_adaptor<range_iterator_t<typename Derived::base_iterable_t const>>;

        template<typename Derived>
        using base_sentinel_t =
            detail::basic_adaptor<range_sentinel_t<typename Derived::base_iterable_t const>>;

        template<typename Derived>
        using derived_cursor_t =
            decltype(range_core_access::begin_adaptor(std::declval<Derived const &>())
                .begin(std::declval<Derived const &>()));

        template<typename Derived>
        using derived_sentinel_t =
            decltype(range_core_access::end_adaptor(std::declval<Derived const &>())
                .end(std::declval<Derived const &>()));

        struct adaptor_defaults : private range_core_access
        {
            using range_core_access::equal;
            using range_core_access::empty;
            using range_core_access::current;
            using range_core_access::next;
            using range_core_access::prev;
            using range_core_access::advance;
            using range_core_access::distance_to;
            template<typename Range>
            static base_cursor_t<Range> begin(Range const &rng)
            {
                return rng.base_begin();
            }
            template<typename Range>
            static base_sentinel_t<Range> end(Range const &rng)
            {
                return rng.base_end();
            }
        };

        template<typename Derived, typename BaseIterable, bool Infinite>
        struct range_adaptor
          : range_facade<Derived, Infinite>
        {
        private:
            friend Derived;
            friend range_core_access;
            friend adaptor_defaults;
            using range_adaptor_t = range_adaptor;
            using base_iterable_t = BaseIterable;
            using range_facade<Derived, Infinite>::derived;
            BaseIterable rng_;

            template<typename Adaptor, typename BaseCursorOrSentinel>
            struct cursor_or_sentinel : private Adaptor
            {
            private:
                template<typename A, typename I>
                friend struct cursor_or_sentinel;
                friend struct range_adaptor;
                BaseCursorOrSentinel base_;
                cursor_or_sentinel(Adaptor adaptor, BaseCursorOrSentinel base)
                  : Adaptor(std::move(adaptor)), base_(std::move(base))
                {}
                Adaptor &adaptor()
                {
                    return *this;
                }
                Adaptor const &adaptor() const
                {
                    return *this;
                }
            public:
                using single_pass = std::integral_constant<bool,
                    range_core_access::single_pass_t<Adaptor>::value ||
                        range_core_access::single_pass_t<BaseCursorOrSentinel>::value>;
                cursor_or_sentinel() = default;
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().current(base_))>
                R current() const
                {
                    return adaptor().current(base_);
                }
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().next(base_))>
                void next()
                {
                    adaptor().next(base_);
                }
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().equal(base_, base_))>
                bool equal(cursor_or_sentinel const &that) const
                {
                    return adaptor().equal(base_, that.base_);
                }
                template<typename A = Adaptor,
                         typename A2, typename I,
                         typename R = decltype(std::declval<A>().empty(std::declval<I const &>(), base_))>
                constexpr bool equal(cursor_or_sentinel<A2, I> const &that) const
                {
                    // "that" is the iterator, "this" is the sentinel
                    CONCEPT_ASSERT(Same<I, derived_cursor_t<Derived>>() &&
                                   Same<A2, detail::cursor_adaptor_t<Derived>>());
                    return adaptor().empty(that.base_, base_);
                }
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().prev(base_))>
                void prev()
                {
                    adaptor().prev(base_);
                }
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().advance(base_, 0))>
                void advance(range_difference_t<BaseIterable> n)
                {
                    adaptor().advance(base_, n);
                }
                template<typename A = Adaptor,
                         typename R = decltype(std::declval<A>().distance_to(base_, base_))>
                range_difference_t<BaseIterable>
                distance_to(cursor_or_sentinel const &that) const
                {
                    return adaptor().distance_to(base_, that.base_);
                }
            };

            adaptor_defaults get_adaptor(begin_end_tag) const
            {
                return {};
            }

            base_cursor_t<range_adaptor> base_begin() const
            {
                return {ranges::begin(rng_)};
            }
            base_sentinel_t<range_adaptor> base_end() const
            {
                return {ranges::end(rng_)};
            }
            CONCEPT_REQUIRES(SizedIterable<BaseIterable>())
            range_size_t<BaseIterable> base_size() const
            {
                return ranges::size(rng_);
            }

            template<typename D = Derived>
            cursor_or_sentinel<detail::cursor_adaptor_t<D>, derived_cursor_t<D>>
            get_begin() const
            {
                auto adaptor = range_core_access::begin_adaptor(derived());
                auto pos = adaptor.begin(derived());
                return {std::move(adaptor), std::move(pos)};
            }

            template<typename D = Derived>
            cursor_or_sentinel<detail::sentinel_adaptor_t<D>, derived_sentinel_t<D>>
            get_end() const
            {
                auto adaptor = range_core_access::end_adaptor(derived());
                auto pos = adaptor.end(derived());
                return {std::move(adaptor), std::move(pos)};
            }
        public:
            constexpr range_adaptor(BaseIterable && rng)
              : rng_(detail::forward<BaseIterable>(rng))
            {}
        };
    }
}

#endif
