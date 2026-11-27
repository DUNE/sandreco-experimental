/**
 * @file
 * @brief Implementation of the magnetic field of @ref sand::geoinfo (GDML field tags, uniform and volume fields)
 *        and of @ref sand::geoinfo::field_probe.
 */

#include <geoinfo/field_info.hpp>

#include <root_tgeomanager/root_tgeomanager.hpp>

#include <ufw/config.hpp>
#include <ufw/utils.hpp>

#include <TGeoCache.h>
#include <TGeoManager.h>
#include <TGeoNavigator.h>
#include <TGeoNode.h>
#include <TGeoVolume.h>
#include <TXMLEngine.h>

#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <set>
#include <utility>

namespace sand {

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

    /// Throw if cfg has a key that is not in allowed.
    void check_keys(const ufw::json& cfg, std::string_view where, std::initializer_list<std::string_view> allowed) {
      for (auto const& item : cfg.items()) {
        bool known = false;
        for (auto const& a : allowed) {
          known = known || item.key() == a;
        }
        if (!known) {
          UFW_ERROR("sand::geoinfo: unknown key '{}' in the '{}' configuration.", item.key(), where);
        }
      }
    }

  } // namespace

  dir_3d parse_bfield(std::string_view value) {
    std::string_view s = trim(value);
    if (!s.empty() && s.front() == '(') {
      s.remove_prefix(1);
    }
    if (!s.empty() && s.back() == ')') {
      s.remove_suffix(1);
    }
    double b[3] = {0., 0., 0.};
    for (int i = 0; i < 3; ++i) {
      auto const comma = s.find(',');
      if ((i < 2) == (comma == std::string_view::npos)) {
        UFW_ERROR("Malformed BField value '{}': expected three comma-separated components.", value);
      }
      b[i] = parse_component(s.substr(0, comma), value);
      s    = i < 2 ? s.substr(comma + 1) : std::string_view();
    }
    return dir_3d(b[0], b[1], b[2]);
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

  // ---------------------------------------------------------------------------------------------------------------
  // field_info

  geoinfo::field_info::~field_info() = default;

  std::unique_ptr<const geoinfo::field_info> geoinfo::field_info::create(const ufw::json& f, root_tgeomanager& geo) {
    if (f.is_array()) {
      if (f.size() != 3 || !f[0].is_number() || !f[1].is_number() || !f[2].is_number()) {
        UFW_ERROR("sand::geoinfo: 'field' must be an array of three numbers [T], got {}.", f.dump());
      }
      return std::make_unique<uniform_field>(dir_3d(f[0].get<double>(), f[1].get<double>(), f[2].get<double>()));
    }
    if (f.is_object()) {
      check_keys(f, "field", {"source", "file"});
      if (!f.contains("source") || f.at("source") != "gdml") {
        UFW_ERROR("sand::geoinfo: 'field.source' must be \"gdml\", got {}.",
                  f.contains("source") ? f.at("source").dump() : std::string("nothing"));
      }
      if (!f.contains("file")) {
        UFW_ERROR("sand::geoinfo: missing required key 'field.file'.");
      }
      if (!f.at("file").is_string()) {
        UFW_ERROR("sand::geoinfo: 'field.file' must be a path, got {}.", f.at("file").dump());
      }
      // Relative paths are resolved against ufw-basepath, as for the geometry of root_tgeomanager
      auto const file = ufw::config(f).path_at("file");
      return std::make_unique<volume_field>(geo, read_gdml_field_tags(file), file.string());
    }
    UFW_ERROR("sand::geoinfo: 'field' must be an array [Bx, By, Bz] in T or an object "
              "{{ \"source\" : \"gdml\", \"file\" : <path> }}, got {}.",
              f.dump());
  }

  // ---------------------------------------------------------------------------------------------------------------
  // uniform_field

  std::string geoinfo::uniform_field::description() const {
    return fmt::format("uniform field ({}, {}, {}) T", m_b.X(), m_b.Y(), m_b.Z());
  }

  // ---------------------------------------------------------------------------------------------------------------
  // volume_field

  geoinfo::volume_field::volume_field(root_tgeomanager& geo, const std::vector<gdml_field_tag>& tags,
                                      std::string source)
    : m_geo(&geo), m_manager(gGeoManager), m_source(std::move(source)) {
    // root_tgeomanager allows a single TGeoManager, which is gGeoManager
    if (!m_manager || !m_manager->GetTopVolume()) {
      UFW_ERROR("sand::geoinfo::volume_field: no geometry loaded.");
    }

    // Own tag of each tagged volume, by index in tags
    std::unordered_map<const TGeoVolume*, std::size_t> own;
    m_usage.resize(tags.size());
    for (std::size_t i = 0; i != tags.size(); ++i) {
      TGeoVolume const* v = m_manager->GetVolume(tags[i].volume.c_str());
      if (!v) {
        v = m_manager->GetVolume((tags[i].volume + "_PV").c_str());
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
    std::vector<std::pair<const TGeoVolume*, std::size_t>> todo{{m_manager->GetTopVolume(), no_tag}};
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
                    v->GetName(), tags[found].volume, tags[tag].volume, a.X(), a.Y(), a.Z(), b.X(), b.Y(), b.Z());
        }
        found = tag;
      }
      m_table[v] = found == no_tag ? dir_3d() : tags[found].b;
      if (found == no_tag) {
        ++m_untagged;
      } else if (auto t = own.find(v); t != own.end()) {
        m_usage[t->second].placed = true; // own tag
      } else {
        ++m_usage[found].inheriting; // with several equal fields, counted under one of them
      }
    }

    UFW_INFO("sand::geoinfo::volume_field: {} field tags, {} of {} logical volumes with a non-zero field.", tags.size(),
             magnetised_volumes(), m_table.size());
    for (auto const& u : m_usage) {
      if (u.placed) {
        UFW_INFO("  tag of '{}' (volume '{}'): ({}, {}, {}) T, inherited by {} other logical volumes.", u.volume,
                 u.geometry_volume, u.b.X(), u.b.Y(), u.b.Z(), u.inheriting);
      } else {
        UFW_INFO("  tag of '{}' (volume '{}'): ({}, {}, {}) T, volume not placed in the geometry tree: unused.",
                 u.volume, u.geometry_volume, u.b.X(), u.b.Y(), u.b.Z());
      }
    }
    UFW_INFO("  {} logical volumes without a tagged ancestor: zero field.", m_untagged);
  }

  geoinfo::volume_field::~volume_field() = default;

  dir_3d geoinfo::volume_field::at(const pos_3d& p) const {
    auto nav                   = m_geo->navigator();
    TGeoNode const* const node = nav->FindNode(p.X(), p.Y(), p.Z());
    if (!node || nav->IsOutside()) {
      return dir_3d();
    }
    return volume_value(nav->GetCurrentVolume());
  }

  std::string geoinfo::volume_field::description() const {
    return fmt::format("field of the geometry volumes, {} tagged volumes from '{}'", m_usage.size(), m_source);
  }

  dir_3d geoinfo::volume_field::volume_value(const TGeoVolume* v) const {
    auto const it = m_table.find(v);
    return it == m_table.end() ? dir_3d() : it->second;
  }

  std::size_t geoinfo::volume_field::magnetised_volumes() const {
    std::size_t n = 0;
    for (auto const& [v, b] : m_table) {
      n += b != dir_3d();
    }
    return n;
  }

  // ---------------------------------------------------------------------------------------------------------------
  // field_probe

  geoinfo::field_probe::field_probe()  = default;
  geoinfo::field_probe::~field_probe() = default;

  // A move leaves the source unbound, instead of holding the field pointers without a navigator.
  geoinfo::field_probe::field_probe(field_probe&& other) noexcept
    : m_field(std::exchange(other.m_field, nullptr)),
      m_volumes(std::exchange(other.m_volumes, nullptr)),
      m_nav(std::move(other.m_nav)),
      m_last_volume(std::exchange(other.m_last_volume, nullptr)),
      m_last_b(other.m_last_b) {}

  geoinfo::field_probe& geoinfo::field_probe::operator= (field_probe&& other) noexcept {
    if (this != &other) {
      m_field       = std::exchange(other.m_field, nullptr);
      m_volumes     = std::exchange(other.m_volumes, nullptr);
      m_nav         = std::move(other.m_nav);
      m_last_volume = std::exchange(other.m_last_volume, nullptr);
      m_last_b      = other.m_last_b;
    }
    return *this;
  }

  geoinfo::field_probe::field_probe(const field_info& f)
    : m_field(&f), m_volumes(dynamic_cast<const volume_field*>(&f)) {
    if (!m_volumes) {
      return;
    }
    // A navigator of our own, not registered in the TGeoManager: creating it does not change the current navigator
    // returned by root_tgeomanager::navigator(), and other code moving that navigator does not affect this one.
    TGeoManager* const manager = m_volumes->manager();
    m_nav                      = std::make_unique<TGeoNavigator>(manager);
    m_nav->BuildCache(kTRUE, kFALSE);
    if (manager->IsClosed()) {
      m_nav->GetCache()->BuildInfoBranch();
    }
    m_nav->FindNode(0., 0., 0.);
    m_last_volume = m_nav->IsOutside() ? nullptr : m_nav->GetCurrentVolume();
    m_last_b      = m_volumes->volume_value(m_last_volume);
  }

  dir_3d geoinfo::field_probe::field_at(const pos_3d& p) {
    if (!m_volumes) {
      if (!m_field) {
        UFW_ERROR("sand::geoinfo::field_probe: unbound probe; obtain one from sand::geoinfo::make_field_probe().");
      }
      return m_field->at(p);
    }
    m_nav->IsSameLocation(p.X(), p.Y(), p.Z(), kTRUE);
    if (m_nav->IsOutside()) {
      return dir_3d();
    }
    TGeoVolume const* const v = m_nav->GetCurrentVolume();
    if (v != m_last_volume) {
      m_last_volume = v;
      m_last_b      = m_volumes->volume_value(v);
    }
    return m_last_b;
  }

} // namespace sand
