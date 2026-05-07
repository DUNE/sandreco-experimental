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
template <typename T = sand::truth_index>
class cluster : public sand::truth<T> {
  public:
    cluster() = default;
    explicit cluster(std::vector<sand::reco::digi<>> ds) : m_digit_indices(std::move(ds)) {}
    explicit cluster(sand::reco::digi<> d) : m_digit_indices({d}) {}
    
    cluster(const cluster&) = default;
    cluster(cluster&&) = default;
    cluster& operator=(const cluster&) = default;
    cluster& operator=(cluster&&) = default;
    ~cluster() = default;
    

    const std::vector<sand::reco::digi<>>& digits () const {return m_digit_indices;}
    std::vector<sand::reco::digi<>>& digits_mut() { return m_digit_indices; }

    bool contains(const sand::reco::digi<>& d) const {
        return std::find(m_digit_indices.begin(),
                         m_digit_indices.end(),
                         d) != m_digit_indices.end();
    }

    // Modifiers
    void add_digit(sand::reco::digi<> d) {
        m_digit_indices.push_back(d);
    }
    

  private:
    /// Using shared ptrs to avoid copying, but allow digits to belong to multiple clusters
    std::vector<sand::reco::digi<>> m_digit_indices;
  };


} // namespace sand::reco
