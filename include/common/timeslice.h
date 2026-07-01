#include <common/timerange.h>

namespace sand::reco {

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
    const std::vector<timerange>& slices() const { return m_slices; }

    // inefficient, easy
    template <typename T>
    std::vector<std::vector<T>> slice(const std::vector<T>&) const;

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

   private:
    std::vector<timerange> m_slices;
  };

} // namespace sand::reco
