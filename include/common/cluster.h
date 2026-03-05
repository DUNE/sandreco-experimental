#pragma once

#include <common/sand.h>
#include <common/timerange.h>
#include <common/truth.h>
#include <common/digi.h>
#include <cmath>

namespace sand::reco {

  /**
   * The cluster class represents a generic cluster in the detector.
   * Specialized classes for each detector must inherit from cluster.
   */
class cluster : public sand::truth {
  public:
    cluster() = default;
    explicit cluster(std::vector<size_t> indices) : m_digit_indices(std::move(indices)) {}
    explicit cluster(size_t index) : m_digit_indices({index}) {}
    
    cluster(const cluster&) = default;
    cluster(cluster&&) = default;
    cluster& operator=(const cluster&) = default;
    cluster& operator=(cluster&&) = default;
    ~cluster() = default;
    

    const std::vector<size_t>& digits () const {return m_digit_indices;}
    std::vector<size_t>& digits_mut() { return m_digit_indices; }

    bool contains(size_t idx) const {
        return std::find(m_digit_indices.begin(),
                         m_digit_indices.end(),
                         idx) != m_digit_indices.end();
    }

    // Modifiers
    void add_digit(size_t idx) {
        m_digit_indices.push_back(idx);
    }
    

  private:
    /// Using shared ptrs to avoid copying, but allow digits to belong to multiple clusters
    std::vector<size_t> m_digit_indices;
  };


} // namespace sand::reco
