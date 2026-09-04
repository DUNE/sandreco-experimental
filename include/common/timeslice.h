
#pragma once

#include <common/digi.h>
#include <common/sand.h>
#include <common/timerange.h>

#include <type_traits>

namespace sand::reco {

  template <typename T, typename = void>
  struct time_ordered : std::false_type {};

  template <typename T>
  struct time_ordered<T, std::void_t<decltype(std::declval<T>().t())>> : std::true_type {};

  template <typename T>
  constexpr bool is_time_ordered = time_ordered<T>::value;

  class timeslices : managed_data_base {
   public:
    template <typename It>
    struct timeslice : public timerange {
      It begin() const { return m_begin; }
      It end() const { return m_end; }
      It m_begin;
      It m_end;
    };

   public:
    timeslices(std::vector<timerange> tr) : m_slices(std::move(tr)) {}

    const std::vector<timerange>& slices() const { return m_slices; }
    /*
        // inefficient, easy
        template <typename T>
        std::enable_if_t<is_time_ordered<T>, std::vector<std::vector<T>>> slice(const std::vector<T>& data) const {
          std::vector<std::vector<T>> data_slices;
          data_slices.reserve(m_slices.size());
          auto it = data.begin();
          for (timerange tr : m_slices) {
            while (
          }
        }

        // inefficient, easy
        template <typename T>
        std::vector<std::vector<T>> slice_unsorted(const std::vector<T>& data) const {
          std::vector<std::vector<T>> data_slices;
          data_slices.reserve(m_slices.size());
          for (timerange tr : m_slices) {
          }
        }

        // efficient, lambda, closer to PHLEX
        template <typename T, typename C, typename Fn>
        void for_each(const C<T>& container, Fn&& fn) const {
          for_each(container.begin(), container.end(), std::forward<Fn>(fn));
        }

        // efficient, lambda, closer to PHLEX
        template <typename It, typename Fn>
        void for_each(It begin, It end, Fn&& fn) const {
          auto slice = m_slices.begin();
          while (true) {
            It first = begin;
            while (first != end && *first < slice->earliest()) {
              ++first;
            }
            It last = first;
            while (last != end && *end < slice->latest()) {
              ++last;
            }
            fn(timeslice<It>{slice, first, last});
          }
        }
    */
   private:
    std::vector<timerange> m_slices;
  };

} // namespace sand::reco
