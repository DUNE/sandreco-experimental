#include "EDEPHit.h"


/**
 * @brief Map associating component enums with their string representations.
 */
std::map<component, std::string> component_to_string = {
  { component::GRAIN,  "LArHit"         },
  { component::STRAW,  "Straw"          },
  { component::DRIFT,  "DriftVolume"    },
  { component::ECAL,   "EMCalSci"       },
  { component::MAGNET, "Magnet"         },
  { component::WORLD,  "World"          },
};

/**
 * @brief Map associating string representations of components with their enums.
 */
std::map<std::string, component> string_to_component = {
  { "LArHit",         component::GRAIN  },
  { "Straw",          component::STRAW  },
  { "DriftVolume",    component::DRIFT  },
  { "EMCalSci",       component::ECAL   },
  { "Magnet",         component::MAGNET },
  { "World",          component::WORLD  },
};

/**
 * @brief List of components in an array.
 */
 component components[6] = {component::GRAIN, component::STRAW, component::DRIFT, component::ECAL, component::MAGNET, component::WORLD};
 
/**
 * @brief List of string names associated with the GRAIN component.
 */
std::initializer_list<std::string> grain_names   = {"GRAIN", "GRIAN"};

/**
 * @brief List of string names associated with the STRAW component.
 */
std::initializer_list<std::string> stt_names      = {"horizontalST", "STT", "TrkMod", "CMod", "C3H6Mod", "Trk", "SuperMod", "Drift"};

/**
 * @brief List of string names associated with the DRIFT component.
 */
std::initializer_list<std::string> drift_names    = {"SANDtracker", "Trk", "CMod", "C3H6Mod", "SuperMod", "Drift", "C3H6Target", "CTarget", "C3H6Mylar", "CMylar", "Frame"};

/**
 * @brief List of string names associated with the ECAL component.
 */
std::initializer_list<std::string> ecal_names    = {"ECAL", "kloe_calo_volume"};

/**
 * @brief List of string names associated with the MAGNET component.
 */
std::initializer_list<std::string> magnet_names  = {"KLOE", "Yoke", "Mag"};

/**
 * @brief List of string names associated with the WORLD component.
 */
std::initializer_list<std::string> world_names    = {"World", "rock", "volSAND", "sand_inner", "Enclosure", "volDetEnclosure"};
