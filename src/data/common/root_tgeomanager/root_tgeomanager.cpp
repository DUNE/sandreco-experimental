#include <root_tgeomanager.hpp>
#include <ufw/config.hpp>

#include <TGeoManager.h>

namespace sand {

  root_tgeomanager::root_tgeomanager(const ufw::config& cfg) {
    if (gGeoManager) {
      UFW_FATAL("Does not support multiple geomanagers. Existing instance at {}.", fmt::ptr(gGeoManager));
    }
    auto filepath = cfg.path_at("geometry");
    m_geomanager = TGeoManager::Import(filepath.c_str());
    if (!m_geomanager) {
      UFW_ERROR("Cannot find valid TGeoManager in '{}'.", filepath.c_str());
    }
  }

  root_tgeomanager::~root_tgeomanager() {
    if (gGeoManager != m_geomanager) {
      UFW_FATAL("Does not support multiple geomanagers. Existing instance at {}.", fmt::ptr(gGeoManager));
    }
  }

  root_tgeomanager::geonav_deleter::geonav_deleter(root_tgeomanager* p) : m_parent(p) {}

  void root_tgeomanager::geonav_deleter::operator () (tgeonav*) {}

  std::unique_ptr<root_tgeomanager::tgeonav, root_tgeomanager::geonav_deleter> root_tgeomanager::navigator() {
    auto nav = static_cast<tgeonav*>(m_geomanager->GetCurrentNavigator());
    return std::unique_ptr<root_tgeomanager::tgeonav, root_tgeomanager::geonav_deleter>(nav, this);
  }

  root_tgeomanager::path& root_tgeomanager::path::operator /= (const std::string_view& sv) {
    if (empty()) {
      assign(sv);
      return *this;
    }
    if (sv.empty()) {
      return *this;
    }
    if (back() == '/') {
      if (sv.front() == '/') {
        append(sv.substr(1));
      } else {
        append(sv);
      }
    } else {
      if (sv.front() == '/') {
        append(sv);
      } else {
        reserve(size() + 1 + sv.size());
        push_back('/');
        append(sv);
      }
    }
    return *this;
  }

  std::string_view root_tgeomanager::path::token(std::size_t i) const {
    std::size_t start = 0;
    std::size_t stop = 0;
     while (i--) {
       start = find('/', start + 1);
    }
    stop = find('/', start + 1);
    if (stop == std::string::npos) {
      stop = size();
    }
    return std::string_view(data() + start + 1, stop - start - 1);
  }

}
