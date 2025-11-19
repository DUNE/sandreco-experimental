#pragma once

#include <initializer_list>
#include <map>
#include <string>

/// \file

/// @brief Enum representing different components in the detector.
enum class component { GRAIN, STRAW, DRIFT, ECAL, MAGNET, WORLD };

extern component components[6];

extern std::map<component, std::string> component_to_string;

extern std::map<std::string, component> string_to_component;

extern std::initializer_list<std::string> grain_names;
extern std::initializer_list<std::string> stt_names;
extern std::initializer_list<std::string> drift_names;
extern std::initializer_list<std::string> ecal_names;
extern std::initializer_list<std::string> magnet_names;
extern std::initializer_list<std::string> world_names;
