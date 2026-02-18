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

    explicit cluster(const std::shared_ptr<digi>& d) : m_digits({d}) {}
    
    cluster(const cluster&) = default;
    cluster(cluster&&) = default;
    cluster& operator=(const cluster&) = default;
    cluster& operator=(cluster&&) = default;
    ~cluster() = default;
    
    // Accessors
    const std::vector<std::shared_ptr<digi>>& digits() const { return m_digits; }
    std::vector<std::shared_ptr<digi>>& digits_mut() { return m_digits; }

    bool contains(const std::shared_ptr<digi>& d) const {
        return std::find(m_digits.begin(), m_digits.end(), d) != m_digits.end();
    }
    
    // Modifiers
    void add_digit(std::shared_ptr<digi> d) {
        m_digits.push_back(std::move(d));
    }

    void add_digit(std::shared_ptr<digi>&& d) {
        m_digits.push_back(std::move(d));  // Overloaded option to transfer ownership
    }

  private:
    /// Using shared ptrs to avoid copying, but allow digits to belong to multiple clusters
    std::vector<std::shared_ptr<digi>> m_digits;
  };


} // namespace sand::reco
