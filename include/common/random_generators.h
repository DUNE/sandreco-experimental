#pragma once

#include <random>

namespace sand {

  inline double gaussian_error(double mean = 0.0, double stddev = 1.0) {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::normal_distribution<double> distribution(mean, stddev);
    return distribution(generator);
  }

}