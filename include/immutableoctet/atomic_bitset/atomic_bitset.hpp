#pragma once

#include <utility>
#include <type_traits>
#include <atomic>
#include <vector>
#include <memory>
#include <tuple>
#include <array>
#include <iterator>

#include <cstdint>
#include <cstddef>
#include <cassert>

namespace immutableoctet
{
  namespace impl
  {
    template <typename T, std::size_t ...Indices>
    constexpr std::array<std::atomic<T>, sizeof...(Indices)>*
    inplace_construct_atomic_array_impl
    (
      void* memory_region, T value, std::index_sequence<Indices...>
    )
    {
      return new (memory_region) std::array<std::atomic<T>, sizeof...(Indices)> { {(static_cast<void>(Indices), value)...} };
    }

    template <std::size_t array_size, typename T>
    constexpr std::array<std::atomic<T>, array_size>*
    inplace_construct_atomic_array(void* memory_region, T value)
    {
      return inplace_construct_atomic_array_impl
      (
        memory_region,

        value,

        std::make_index_sequence<array_size>()
      );
    }

    template <typename T, typename bit_index_t, typename Operation>
    T update_bit(std::atomic<T>& element, bit_index_t bit, Operation&& operation)
    {
      static_assert((std::is_integral<std::decay_t<T>>::value), "`T` must be an integral type");

      auto value = element.load();

      while (!element.compare_exchange_weak(value, operation(value, bit))) {}

      return value;
    }

    template <typename T, typename bit_index_t>
    T enable_bit(std::atomic<T>& element, bit_index_t bit)
    {
      return update_bit
      (
        element, bit,

        [](T value, bit_index_t bit)
        {
          const auto bitmask = (static_cast<T>(1) << static_cast<T>(bit));

          return (value | bitmask);
        }
      );
    }

    template <typename T, typename bit_index_t>
    T disable_bit(std::atomic<T>& element, bit_index_t bit)
    {
      return update_bit
      (
        element, bit,

        [](T value, bit_index_t bit)
        {
          const auto bitmask = ~(static_cast<T>(1) << static_cast<T>(bit));

          return (value & bitmask);
        }
      );
    }

    template <typename T, typename bit_index_t, typename value_type=bool>
    T set_bit(std::atomic<T>& element, bit_index_t bit, value_type value)
    {
      return (value)
        ? enable_bit(element, bit)
        : disable_bit(element, bit)
      ;
    }

    template <typename T, typename bit_index_t>
    T toggle_bit(std::atomic<T>& element, bit_index_t bit)
    {
      return update_bit
      (
        element, bit,

        [](T value, bit_index_t bit)
        {
          const auto bitmask = (static_cast<T>(1) << static_cast<T>(bit));

          return (value ^ bitmask);
        }
      );
    }

    template <typename T, typename bit_index_t>
    T get_bit(const std::atomic<T>& element, bit_index_t bit)
    {
      static_assert((std::is_integral<std::decay_t<T>>::value), "`T` must be an integral type");

      const auto value = element.load();

      const auto bitmask = (static_cast<T>(1) << static_cast<T>(bit));

      return (value & bitmask);
    }
  }

  template <typename T, std::size_t page_size, typename AtomicType=std::atomic<T>>
  class fixed_size_atomic_page
  {
    public:
      //static_assert(std::is_trivially_copyable_v<T>);

      using size_t = std::size_t;
      using index_t = size_t;

      using atomic_type = AtomicType;
      using element_type = atomic_type;
      using array_type = std::array<element_type, page_size>;

      using value_type = T;
      using underlying_type = value_type;

      using reference = element_type&;
      using const_reference = const element_type&;

      using pointer_type = std::unique_ptr<array_type>; // std::shared_ptr<array_type>;
      using raw_pointer_type = element_type*; // std::decay_t<array_type>;
      using const_raw_pointer_type = const element_type*; // std::decay_t<array_type>;

      inline static constexpr size_t page_size_in_memory = (sizeof(array_type)); // (sizeof(element_type[page_size])); // (page_size * sizeof(element_type));

      // Default initializes the page.
      fixed_size_atomic_page() :
        page_content(std::make_unique<array_type>())
      {}

      // Initializes all values in the page to a copy of `value`.
      fixed_size_atomic_page(value_type value) :
        page_content(allocate_memory_block())
      {
        auto allocated_region = page_content.get();

        impl::inplace_construct_atomic_array<page_size, value_type>(allocated_region, value);
      }

      fixed_size_atomic_page(fixed_size_atomic_page&& other) noexcept = default;
      fixed_size_atomic_page& operator=(fixed_size_atomic_page&& other) noexcept = default;

      fixed_size_atomic_page(const fixed_size_atomic_page&) = delete;
      fixed_size_atomic_page& operator=(const fixed_size_atomic_page&) = delete;

      const const_raw_pointer_type data() const
      {
        return page_content.get()->data();
      }

      raw_pointer_type data()
      {
        return page_content.get()->data();
      }

      constexpr size_t size() const
      {
        return page_size;
      }

      reference operator[](index_t index)
      {
        return page_content[index];
      }

      const_reference operator[](index_t index) const
      {
        return page_content[index];
      }

    protected:
      pointer_type page_content;

    private:
      static array_type* allocate_memory_block()
      {
        return reinterpret_cast<array_type*>(::operator new(page_size_in_memory));
      }
  };

  template <typename T, typename BitOffsetType, typename AtomicType=std::atomic<T>, typename PointerType=AtomicType*>
  class atomic_bit_reference
  {
    public:
      using atomic_type = AtomicType;
      using underlying_type = T;
      using value_type = bool;
      using pointer_type = PointerType;
      using const_pointer_type = const PointerType;

      using reference = decltype(*std::declval<pointer_type>());
      using const_reference = decltype(*std::declval<const_pointer_type>());

      using bit_offset_t = BitOffsetType;

      inline static constexpr bool is_const = std::is_same_v<pointer_type, const_pointer_type>; // std::is_same_v<reference, const_reference>;

      atomic_bit_reference() :
        remote_value(nullptr),
        bit_offset()
      {}

      atomic_bit_reference(reference instance, bit_offset_t bit_offset) :
        remote_value(&instance),
        bit_offset(bit_offset)
      {}

      atomic_bit_reference(const atomic_bit_reference&) = default;
      atomic_bit_reference(atomic_bit_reference&&) noexcept = default;

      atomic_bit_reference& operator=(const atomic_bit_reference&) = default;
      atomic_bit_reference& operator=(atomic_bit_reference&&) noexcept = default;

      atomic_bit_reference& operator=(value_type value)
      {
        impl::set_bit(get_underlying(), get_bit_offset(), value);

        return *this;
      }

      value_type get() const
      {
        if (!remote_value)
        {
          return {};
        }

        return impl::get_bit(get_underlying(), get_bit_offset());
      }

      reference get_underlying()
      {
        assert(remote_value);

        return *remote_value;
      }

      const_reference get_underlying() const
      {
        assert(remote_value);

        return *remote_value;
      }

      bit_offset_t get_bit_offset() const
      {
        return bit_offset;
      }

      operator value_type() const // explicit
      {
        return get();
      }

    protected:
      pointer_type remote_value;
      bit_offset_t bit_offset;
  };

  template <typename T, typename BitOffsetType, typename AtomicType=std::atomic<T>, typename PointerType=const AtomicType*>
  using atomic_bit_const_reference = atomic_bit_reference<T, BitOffsetType, AtomicType, PointerType>;

  template
  <
    // Specifies the underlying integral type used to store binary data.
    typename T,

    // Controls the number of elements allocated for each page of memory.
    std::size_t fixed_page_size,

    // Specifies the default value to be applied to new pages' elements.
    T default_element_value=T{},

    // If enabled, initialization of pages will be performed using `default_element_value` for each element.
    bool default_initialize=true
  >
  class basic_atomic_bitset
  {
    public:
      using underlying_type = T;
      
      using atomic_type    = std::atomic<underlying_type>;
      using element_type   = atomic_type;
      using page_type      = fixed_size_atomic_page<underlying_type, fixed_page_size, element_type>; // page_size
      using container_type = std::vector<page_type>; // std::vector<element_type>;

      using value_type = bool;

      using size_t = std::size_t;

      using index_t      = size_t;
      using page_index_t = size_t;
      using element_index_t = size_t;
      using bit_index_t  = index_t;

      using bit_location = std::tuple
      <
        page_index_t,    // The heap-allocated memory page a bit belongs to.
        element_index_t, // The element of the page storing the bit.
        bit_index_t      // The bitwise offset into the element where the value is stored.
      >;

      using reference       = atomic_bit_reference<T, bit_index_t>;
      using const_reference = atomic_bit_const_reference<T, bit_index_t>;

      inline static constexpr size_t page_size = static_cast<size_t>(fixed_page_size);

      inline static constexpr size_t bits_per_byte = 8;
      inline static constexpr size_t bit_stride    = (sizeof(underlying_type) * bits_per_byte);
      inline static constexpr size_t page_stride   = (static_cast<size_t>(page_size) * bit_stride);

      inline static constexpr bool is_atomic = true; // std::is_same_v<std::decay_t<element_type>, std::atomic<underlying_type>>;

      template <bool is_const>
      class iterator_impl
      {
        public:
          using iterator_category = std::forward_iterator_tag;
          using difference_type = index_t; // std::ptrdiff_t;
          using value_type = bool;

          using reference = atomic_bit_reference<T, bit_index_t>;
          using const_reference = atomic_bit_const_reference<T, bit_index_t>;

          //using pointer = reference*;

          using container_ptr = std::conditional_t<is_const, const basic_atomic_bitset*, basic_atomic_bitset*>;
          using container_reference = std::conditional_t<is_const, const basic_atomic_bitset&, basic_atomic_bitset&>;

          iterator_impl(container_reference target_bitset, index_t index) :
            target_bitset(&target_bitset), index(index) {}

          iterator_impl& operator++()
          {
            if (index_is_valid())
            {
              index++;
            }

            return *this;
          }

          iterator_impl& operator--()
          {
            if ((index) && (index_is_valid_or_end()))
            {
              index--;
            }

            return *this;
          }

          std::enable_if_t<!is_const, reference> operator*()
          {
            assert(target_bitset);
            //assert(!index_is_end());

            return target_bitset->get_reference(index);
          }

          const_reference operator*() const
          {
            assert(target_bitset);
            //assert(!index_is_end());

            return target_bitset->get_reference(index);
          }

          auto operator<=>(const iterator_impl&) const = default;
          
        protected:
          bool index_is_valid() const
          {
            return (index < size());
          }

          bool index_is_end() const
          {
            return (index == size());
          }

          bool index_is_valid_or_end() const
          {
            return (index <= size()); // (index_is_valid() || (index_is_end))
          }

          const size_t size() const
          {
            assert(target_bitset);

            return target_bitset->size();
          }

          container_ptr target_bitset;

          index_t index;
      };

      using iterator = iterator_impl<false>;
      using const_iterator = iterator_impl<true>;

      static constexpr page_index_t resolve_page_index(index_t index)
      {
        return (static_cast<page_index_t>(index) / static_cast<page_index_t>(page_stride));
      }

      static constexpr element_index_t resolve_element_index(index_t index)
      {
        return ((static_cast<element_index_t>(index) / static_cast<element_index_t>(bit_stride)) % static_cast<element_index_t>(page_size));
      }

      static constexpr bit_index_t resolve_bit_offset_from_index(index_t index)
      {
        return static_cast<bit_index_t>(index % static_cast<index_t>(bit_stride));
      }

      static constexpr bit_location resolve_index(index_t index)
      {
        const auto page_index = resolve_page_index(index);
        const auto underlying_index = resolve_element_index(index);
        const auto bit_index = resolve_bit_offset_from_index(index);

        return { page_index, underlying_index, bit_index };
      }

      basic_atomic_bitset() = default;

      basic_atomic_bitset(basic_atomic_bitset&&) noexcept = default;
      basic_atomic_bitset& operator=(basic_atomic_bitset&&) noexcept = default;

      basic_atomic_bitset(const basic_atomic_bitset&) = delete;
      basic_atomic_bitset& operator=(const basic_atomic_bitset&) = delete;

      page_type& get_page(index_t index)
      {
        const auto page_index = resolve_page_index(index);

        return pages[index];
      }

      const page_type& get_page(index_t index) const
      {
        const auto page_index = resolve_page_index(index);

        return pages[index];
      }

      element_type* try_get_element(index_t index)
      {
        const auto page_index = resolve_page_index(index);
        auto* page_data = get_page_data(page_index);

        if (!page_data)
        {
          return {};
        }

        const auto element_index = resolve_element_index(index);

        auto& element = page_data[element_index];

        return &element;
      }

      const element_type* try_get_element(index_t index) const
      {
        const auto page_index = resolve_page_index(index);
        const auto* page_data = get_page_data(page_index);

        if (!page_data)
        {
          return {};
        }

        const auto element_index = resolve_element_index(index);
        const auto& element = page_data[element_index];

        return &element;
      }

      element_type& get_element(index_t index)
      {
        auto* element = try_get_element(index);

        assert(element);

        return (*element);
      }

      const element_type& get_element(index_t index) const
      {
        const auto* element = try_get_element(index);

        assert(element);

        return (*element);
      }

      reference get_reference(index_t index)
      {
        auto* element = try_get_element(index);

        if (!element)
        {
          return {};
        }

        const auto bit_offset = resolve_bit_offset_from_index(index);

        return reference { *element, bit_offset };
      }

      const_reference get_reference(index_t index) const
      {
        const auto* element = try_get_element(index);

        if (!element)
        {
          return {};
        }

        const auto bit_offset = resolve_bit_offset_from_index(index);

        return const_reference { *element, bit_offset };
      }

      value_type get(index_t index) const
      {
        const auto* element = try_get_element(index);

        if (!element)
        {
          return {};
        }

        const auto bit_offset = resolve_bit_offset_from_index(index);

        return impl::get_bit(*element, bit_offset);
      }

      underlying_type set(index_t index, value_type value)
      {
        const auto bit_offset = resolve_bit_offset_from_index(index);

        auto& element = get_element(index);

        return impl::set_bit(element, bit_offset, value);
      }

      underlying_type enable(index_t index)
      {
        const auto bit_offset = resolve_bit_offset_from_index(index);

        auto& element = get_element(index);

        return impl::enable_bit(element, bit_offset);
      }

      underlying_type disable(index_t index)
      {
        const auto bit_offset = resolve_bit_offset_from_index(index);

        auto& element = get_element(index);

        return impl::disable_bit(element, bit_offset);
      }

      underlying_type toggle(index_t index)
      {
        const auto bit_offset = resolve_bit_offset_from_index(index);

        auto& element = get_element(index);

        return impl::toggle_bit(element, bit_offset);
      }

      underlying_type speculative_set(index_t index, value_type value)
      {
        request_index(index);

        return set(index, value);
      }

      underlying_type speculative_enable(index_t index)
      {
        request_index(index);

        return enable(index);
      }

      underlying_type speculative_disable(index_t index)
      {
        request_index(index);

        return disable(index);
      }

      underlying_type speculative_toggle(index_t index)
      {
        request_index(index);

        return toggle(index);
      }

      value_type speculative_get(index_t index) const
      {
        if (index < size())
        {
          return get(index);
        }

        return {};
      }

      const_reference speculative_get_reference(index_t index) const
      {
        request_index(index);

        return get_reference(index);
      }

      reference speculative_get_reference(index_t index)
      {
        request_index(index);

        return get_reference(index);
      }

      underlying_type emplace_back(value_type value)
      {
        const auto index = next_index();
        
        allocate_pages_for_index(index);

        size_in_bits++;

        return set(index, value);
      }

      underlying_type push_back(value_type value)
      {
        return emplace_back(value);
      }

      value_type pop_back()
      {
        if (empty())
        {
          return {};
        }

        const auto updated_size = (size_in_bits - static_cast<size_t>(1));

        const auto value = get(updated_size);

        size_in_bits = updated_size;

        return value;
      }

      index_t next_index() const
      {
        return static_cast<index_t>(size());
      }

      index_t last_index() const
      {
        if (empty())
        {
          return {};
        }

        return (next_index() - static_cast<index_t>(1));
      }

      size_t page_count() const
      {
        if (size() <= static_cast<size_t>(page_stride))
        {
          return 1;
        }

        return static_cast<size_t>(size() / static_cast<size_t>(page_stride));
      }

      size_t element_count() const
      {
        if (size() <= static_cast<size_t>(bit_stride))
        {
          return 1;
        }

        return static_cast<size_t>(size() / static_cast<size_t>(bit_stride));
      }

      bool empty() const
      {
        return (size() == 0);
      }

      size_t size() const
      {
        //return (element_count() * static_cast<size_t>(bit_stride));

        return size_in_bits;
      }

      size_t capacity() const
      {
        return bits_allocated();
      }

      size_t reserve(size_t requested_size)
      {
        if (requested_size > 0)
        {
          const auto requested_index = (static_cast<index_t>(requested_size) - static_cast<index_t>(1));

          allocate_pages_for_index(requested_index);
        }

        return capacity();
      }

      size_t resize(size_t requested_size)
      {
        if (requested_size == size())
        {
          return size();
        }
        else if (requested_size < size())
        {
          size_in_bits = requested_size;

          // TODO: Look into implementing mass-zeroing of bits.
        }
        else // if (requested_size > size())
        {
          reserve(requested_size);

          size_in_bits = requested_size;
        }

        return size();
      }

      size_t request_index(index_t requested_index)
      {
        const auto index_as_size = static_cast<size_t>(requested_index);

        if (index_as_size >= size())
        {
          allocate_pages_for_index(requested_index);

          size_in_bits = (index_as_size + static_cast<size_t>(1));
        }

        return size();
      }

      void clear()
      {
        size_in_bits = 0;
      }

      const_iterator cbegin() const
      {
        return const_iterator { *this, index_t {} };
      }

      const_iterator cend() const
      {
        return const_iterator { *this, next_index() };
      }

      iterator begin()
      {
        return iterator { *this, index_t {} };
      }

      iterator end()
      {
        return iterator { *this, next_index() };
      }

      const_iterator begin() const
      {
        return cbegin();
      }

      const_iterator end() const
      {
        return cend();
      }

      explicit operator bool() const
      {
        return (!empty());
      }

      /*
      const_reference operator[](index_t index) const
      {
        return speculative_get_reference(index);
      }
      */

      /*
        Pros:
          * Slightly faster/more-optimal
          * Works better with auto
        Cons:
          * Less compatible with common algorithms.
      */
      value_type operator[](index_t index) const
      {
        return get(index);
      }

      reference operator[](index_t index)
      {
        return speculative_get_reference(index);
      }

    protected:
      size_t pages_allocated() const
      {
        return static_cast<size_t>(pages.size());
      }

      size_t elements_allocated() const
      {
        return (pages_allocated() * page_size);
      }

      size_t bits_allocated() const
      {
        return (pages_allocated() * page_stride); // (elements_allocated() * bit_stride);
      }

      element_type* get_page_data(page_index_t page_index)
      {
        if (page_index >= pages_allocated())
        {
          return {};
        }

        const auto native_page_index = static_cast<std::size_t>(page_index);

        return pages[native_page_index].data();
      }

      const element_type* get_page_data(page_index_t page_index) const
      {
        if (page_index >= pages_allocated())
        {
          return {};
        }

        const auto native_page_index = static_cast<std::size_t>(page_index);

        return pages[native_page_index].data();
      }

      size_t resize_pages(size_t pages_to_hold, T default_element_value)
      {
        while (pages_allocated() < pages_to_hold)
        {
          pages.emplace_back(default_element_value);
        }

        return pages_allocated();
      }

      size_t resize_pages(size_t pages_to_hold)
      {
        if constexpr (default_initialize)
        {
          return resize_pages(pages_to_hold, default_element_value);
        }
        else
        {
          pages.resize(static_cast<std::size_t>(pages_to_hold));

          return pages_allocated();
        }
      }

      page_type& allocate_page()
      {
        return pages.emplace_back();
      }

      size_t allocate_pages_up_to(page_index_t page_index)
      {
        const auto page_index_as_size = static_cast<size_t>(page_index);

        if (page_index_as_size >= pages_allocated())
        {
          return resize_pages((page_index_as_size + static_cast<size_t>(1)));
        }

        return {};
      }

      size_t allocate_pages_for_index(index_t index)
      {
        const auto page_index = resolve_page_index(index);

        return allocate_pages_up_to(page_index);
      }

      size_t size_in_bits = {};

      container_type pages;
  };

  // Defaults to 64-bit unsigned integers: 512 x 8 x 8 (4096 bytes, 32768 bits)
  using atomic_bitset = basic_atomic_bitset<std::uint64_t, 512>;
}