
#pragma once

#include <common/digi.h>
#include <common/sand.h>
#include <common/timerange.h>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace sand::reco {

  template <typename T, typename = void>
  struct time_ordered : std::false_type {};

  template <typename T>
  struct time_ordered<T, std::void_t<decltype(std::declval<T>().t())>> : std::true_type {};

  template <typename T>
  constexpr bool is_time_ordered = time_ordered<T>::value;

  class timeslicer {
   public:
    template <typename It>
    struct timeslice : public timerange {
      It begin() const { return m_begin; }
      It end() const { return m_end; }
      It m_begin;
      It m_end;
    };

   public:
    timeslicer(std::vector<timerange> tr) : m_slices(std::move(tr)) {
      if (!std::is_sorted(m_slices.begin(), m_slices.end())) {
        UFW_ERROR("Slices must be sorted");
      }
    }

    const std::vector<timerange>& slices() const { return m_slices; }

    template <typename It>
    std::enable_if_t<is_time_ordered<typename std::iterator_traits<It>::value_type>, std::vector<timeslice<It>>>
    slice(It begin, It end) const {
      std::vector<timeslice<It>> data_slices;
      data_slices.reserve(m_slices.size());
      for (auto s : m_slices) {
        It sb = begin;
        while (sb != end && sb->t() < s.earliest()) {
          ++sb;
        }
        It se = sb;
        while (se != end && se->t() < s.latest()) {
          ++se;
        }
        if (sb != se) {
          timeslice<It> slice{s, sb, se};
          data_slices.emplace_back(slice);
        }
      }
      return data_slices;
    }

    template <typename It>
    std::enable_if_t<is_time_ordered<typename std::iterator_traits<It>::value_type>, std::vector<timeslice<It>>>
    slice_disjoint(It begin, It end) const {
      std::vector<timeslice<It>> data_slices;
      data_slices.reserve(m_slices.size());
      It sb = begin;
      for (auto s : m_slices) {
        while (sb != end && sb->t() < s.earliest()) {
          ++sb;
        }
        It se = sb;
        while (se != end && se->t() < s.latest()) {
          ++se;
        }
        if (sb != se) {
          timeslice<It> slice{s, sb, se};
          data_slices.emplace_back(slice);
          sb = se;
        }
      }
      return data_slices;
    }

   private:
    std::vector<timerange> m_slices;
  };

} // namespace sand::reco
