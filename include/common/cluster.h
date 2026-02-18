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
    explicit cluster(std::vector<std::shared_ptr<digi>> digits) 
        : m_digits(std::move(digits)) {}
    
    cluster(const cluster&) = default;
    cluster(cluster&&) = default;
    cluster& operator=(const cluster&) = default;
    cluster& operator=(cluster&&) = default;
    ~cluster() = default;
    
    // Accessors
    const std::vector<std::shared_ptr<digi>>& digits() const { return m_digits; }
    std::vector<std::shared_ptr<digi>>& digits_mut() { return m_digits; }
    
    // Modifiers
    void add_digit(std::shared_ptr<digi> d) {
        m_digits.push_back(std::move(d));
    }

    void move_digit(std::shared_ptr<digi>&& d) {
        m_digits.push_back(std::move(d));  // Move - transfers ownership
    }

  private:
    /// Using shared ptrs to avoid copying, but allow digits to belong to multiple clusters
    std::vector<std::shared_ptr<digi>> m_digits;
  };


} // namespace sand::reco
