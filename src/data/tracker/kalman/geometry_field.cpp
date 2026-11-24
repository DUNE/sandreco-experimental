#include <kalman/geometry_field.hpp>

#include <ufw/utils.hpp>

#include <TGeoManager.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>
#include <TXMLEngine.h>

#include <cctype>
#include <cstdlib>
#include <set>
#include <utility>

namespace sand::tracker::kalman {

  namespace {

    std::string_view trim(std::string_view s) {
      while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
      }
      while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
      }
      return s;
    }

    /// Value [T] of "<number> [unit]"; a missing unit means tesla, as in edep-sim.
    double parse_component(std::string_view text, std::string_view whole) {
      std::string const s(trim(text));
      char const* begin = s.c_str();
      char* end         = nullptr;
      double const v    = std::strtod(begin, &end);
      if (end == begin) {
        UFW_ERROR("Malformed BField value '{}': '{}' is not a number.", whole, s);
      }
      std::string_view const unit = trim(std::string_view(end));
      if (unit.empty() || unit == "T" || unit == "tesla") {
        return v;
      }
      if (unit == "kG" || unit == "kilogauss") {
        return v * 0.1;
      }
      if (unit == "G" || unit == "gauss") {
        return v * 1e-4;
      }
      UFW_ERROR("Malformed BField value '{}': unknown unit '{}'.", whole, unit);
    }

    /// Value of attribute name of an XML node, empty if absent.
    std::string_view attribute(TXMLEngine& xml, XMLNodePointer_t node, const char* name) {
      char const* a = xml.GetAttr(node, name);
      return a ? std::string_view(a) : std::string_view();
    }

    /// Append the field tags of the volumes found under node (included) to out.
    void collect_tags(TXMLEngine& xml, XMLNodePointer_t node, std::vector<gdml_field_tag>& out) {
      for (; node; node = xml.GetNext(node)) {
        if (std::string_view(xml.GetNodeName(node)) != "volume") {
          collect_tags(xml, xml.GetChild(node), out);
          continue;
        }
        bool tagged = false;
        gdml_field_tag tag;
        tag.volume = attribute(xml, node, "name");
        for (auto aux = xml.GetChild(node); aux; aux = xml.GetNext(aux)) {
          if (std::string_view(xml.GetNodeName(aux)) != "auxiliary") {
            continue;
          }
          auto const type = attribute(xml, aux, "auxtype");
          if (type == "BField") {
            tag.b  = parse_bfield(attribute(xml, aux, "auxvalue"));
            tagged = true;
          } else if (type == "EField") {
            tagged = true; // edep-sim: own field manager with B = 0, unless a BField is also given
          }
        }
        if (tagged) {
          out.push_back(std::move(tag));
        }
      }
    }

  } // namespace

  field_vector parse_bfield(std::string_view value) {
    std::string_view s = trim(value);
    if (!s.empty() && s.front() == '(') {
      s.remove_prefix(1);
    }
    if (!s.empty() && s.back() == ')') {
      s.remove_suffix(1);
    }
    field_vector b{};
    for (int i = 0; i < 3; ++i) {
      auto const comma = s.find(',');
      if ((i < 2) == (comma == std::string_view::npos)) {
        UFW_ERROR("Malformed BField value '{}': expected three comma-separated components.", value);
      }
      b[i] = parse_component(s.substr(0, comma), value);
      s    = i < 2 ? s.substr(comma + 1) : std::string_view();
    }
    return b;
  }

  std::vector<gdml_field_tag> read_gdml_field_tags(const std::filesystem::path& gdml) {
    TXMLEngine xml;
    xml.SetSkipComments(true);
    XMLDocPointer_t doc = xml.ParseFile(gdml.c_str());
    if (!doc) {
      UFW_ERROR("Cannot parse the GDML file '{}'.", gdml.string());
    }
    std::vector<gdml_field_tag> tags;
    try {
      collect_tags(xml, xml.DocGetRootElement(doc), tags);
    } catch (...) {
      xml.FreeDoc(doc);
      throw;
    }
    xml.FreeDoc(doc);
    return tags;
  }

  volume_field::volume_field(sand::root_tgeomanager& geo, const std::vector<gdml_field_tag>& tags)
    : m_nav(geo.navigator()) {
    // root_tgeomanager allows a single TGeoManager, which is gGeoManager and owns the navigator
    TGeoManager* const manager = gGeoManager;
    if (!manager || !m_nav) {
      UFW_ERROR("sand::tracker::kalman::volume_field: no geometry loaded.");
    }

    // Own tag of each tagged volume, by index in tags
    std::unordered_map<const TGeoVolume*, std::size_t> own;
    m_usage.resize(tags.size());
    for (std::size_t i = 0; i != tags.size(); ++i) {
      TGeoVolume const* v = manager->GetVolume(tags[i].volume.c_str());
      if (!v) {
        v = manager->GetVolume((tags[i].volume + "_PV").c_str());
      }
      if (!v) {
        UFW_ERROR("Field tag of GDML volume '{}': neither '{}' nor '{}_PV' is in the loaded geometry.", tags[i].volume,
                  tags[i].volume, tags[i].volume);
      }
      own[v]                     = i;
      m_usage[i].volume          = tags[i].volume;
      m_usage[i].geometry_volume = v->GetName();
      m_usage[i].b               = tags[i].b;
    }

    // Walk the logical hierarchy from the top, carrying the tag of the nearest tagged ancestor (none: no_tag).
    // Visiting each (volume, inherited tag) pair once keeps the walk linear in the number of logical volumes.
    std::size_t constexpr no_tag = static_cast<std::size_t>(-1);
    std::unordered_map<const TGeoVolume*, std::vector<std::size_t>> inherited; // per volume, over its placements
    std::set<std::pair<const TGeoVolume*, std::size_t>> visited;
    std::vector<std::pair<const TGeoVolume*, std::size_t>> todo{{manager->GetTopVolume(), no_tag}};
    while (!todo.empty()) {
      auto [v, tag] = todo.back();
      todo.pop_back();
      if (auto t = own.find(v); t != own.end()) {
        tag = t->second;
      }
      if (!visited.emplace(v, tag).second) {
        continue;
      }
      inherited[v].push_back(tag);
      for (int i = 0; i < v->GetNdaughters(); ++i) {
        todo.emplace_back(v->GetNode(i)->GetVolume(), tag);
      }
    }

    // As in edep-sim, the field belongs to the logical volume and is shared by all its placements: placements without
    // a tagged ancestor take the field of the others. Two different fields would depend on the visiting order.
    for (auto const& [v, list] : inherited) {
      std::size_t found = no_tag;
      for (auto const tag : list) {
        if (tag == no_tag) {
          continue;
        }
        if (found != no_tag && tags[found].b != tags[tag].b) {
          auto const& a = tags[found].b;
          auto const& b = tags[tag].b;
          UFW_ERROR("Volume '{}' inherits two different magnetic fields from the tags of '{}' and '{}': ({}, {}, {}) T "
                    "and ({}, {}, {}) T.",
                    v->GetName(), tags[found].volume, tags[tag].volume, a[0], a[1], a[2], b[0], b[1], b[2]);
        }
        found = tag;
      }
      m_table[v] = found == no_tag ? field_vector{} : tags[found].b;
      if (found == no_tag) {
        ++m_untagged;
      } else if (auto t = own.find(v); t != own.end()) {
        m_usage[t->second].placed = true; // own tag
      } else {
        ++m_usage[found].inheriting; // with several equal fields, counted under one of them
      }
    }

    // Start from a defined navigator state
    m_nav->FindNode(0., 0., 0.);
    m_last_volume = m_nav->GetCurrentVolume();
    m_last_b      = volume_value(m_last_volume);

    UFW_INFO("sand::tracker::kalman::volume_field: {} field tags, {} of {} logical volumes with a non-zero field.",
             tags.size(), magnetised_volumes(), m_table.size());
    for (auto const& u : m_usage) {
      if (u.placed) {
        UFW_INFO("  tag of '{}' (volume '{}'): ({}, {}, {}) T, inherited by {} other logical volumes.", u.volume,
                 u.geometry_volume, u.b[0], u.b[1], u.b[2], u.inheriting);
      } else {
        UFW_INFO("  tag of '{}' (volume '{}'): ({}, {}, {}) T, volume not placed in the geometry tree: unused.",
                 u.volume, u.geometry_volume, u.b[0], u.b[1], u.b[2]);
      }
    }
    UFW_INFO("  {} logical volumes without a tagged ancestor: zero field.", m_untagged);
  }

  volume_field::~volume_field() = default;

  field_vector volume_field::at(double x, double y, double z) const {
    m_nav->IsSameLocation(x, y, z, kTRUE);
    if (m_nav->IsOutside()) {
      return field_vector{};
    }
    TGeoVolume const* const v = m_nav->GetCurrentVolume();
    if (v != m_last_volume) {
      m_last_volume = v;
      m_last_b      = volume_value(v);
    }
    return m_last_b;
  }

  field_vector volume_field::volume_value(const TGeoVolume* v) const {
    auto const it = m_table.find(v);
    return it == m_table.end() ? field_vector{} : it->second;
  }

  std::size_t volume_field::magnetised_volumes() const {
    std::size_t n = 0;
    for (auto const& [v, b] : m_table) {
      n += b != field_vector{};
    }
    return n;
  }

} // namespace sand::tracker::kalman
