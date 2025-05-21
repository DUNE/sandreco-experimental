#pragma once

#include "ufw/utils.hpp"
#include <ufw/data.hpp>
#include <common/sand.h>

#include <TGeoManager.h>
#include <TGeoNavigator.h>

namespace sand {

  /**
   * Force the use of explicit TGeoNavigators, to better isolate algos from each other.
   */
  class root_tgeomanager : ufw::data::base<ufw::data::complex_tag, ufw::data::unique_tag, ufw::data::global_tag> {

  public:
    class tgeonav : public TGeoNavigator {
    //NEVER ADD DATA MEMBERS HERE
    public:
      using TGeoNavigator::TGeoNavigator;
      inline void cd(const geo_path&);
      inline TGeoNode* get_node() const;
      TGeoNode* find_node(pos_3d p) { return FindNode(p.x(), p.y(), p.z()); }
      inline TGeoNode* node_at(pos_3d p);
      pos_3d get_point() const { pos_3d ret; ret.SetCoordinates(GetCurrentPoint()); return ret; }
      void set_point(pos_3d p) { SetCurrentPoint(p.x(), p.y(), p.z()); }
      void set_direction(dir_3d d) { SetCurrentDirection(d.x(), d.y(), d.z()); }
      void set_track(pos_3d p, dir_3d d) { InitTrack(p.x(), p.y(), p.z(), d.x(), d.y(), d.z()); }
      inline pos_3d to_local(pos_3d) const;
      inline pos_3d to_master(pos_3d) const;
    };

  private:
    struct geonav_deleter {
      geonav_deleter(root_tgeomanager*);
      void operator () (tgeonav*);
    private:
      root_tgeomanager* m_parent;
    };

    class path_not_found : public ufw::exception {
    public:
      path_not_found(const geo_path& p) : exception("Cannot find path '{}' in geometry.", p.c_str()) {}
    };

    class invalid_position : public ufw::exception {
    public:
      invalid_position(pos_3d pos) : exception("No object found at position ({}, {}, {}).", pos.x(), pos.y(), pos.z()) {}
      invalid_position(std::string_view type, pos_3d pos) : exception("No {} found at position ({}, {}, {}).", type, pos.x(), pos.y(), pos.z()) {}
    };

  public:
    root_tgeomanager(const ufw::config&);

    ~root_tgeomanager();

    std::unique_ptr<tgeonav, geonav_deleter> navigator();

  private:
    TGeoManager* m_geomanager;

  };

  inline void root_tgeomanager::tgeonav::cd(const geo_path& p) {
    if (!TGeoNavigator::cd(p.c_str())) {
      UFW_EXCEPT(path_not_found, p);
    }
  }

  inline TGeoNode* root_tgeomanager::tgeonav::get_node() const {
    TGeoNode* n = GetCurrentNode();
    if (!n) {
      UFW_EXCEPT(invalid_position, get_point());
    }
    return n;
  }

  inline TGeoNode* root_tgeomanager::tgeonav::node_at(pos_3d p) {
    TGeoNode* n = GetCurrentNode();
    if (!n) {
      UFW_EXCEPT(invalid_position, p);
    }
    return n;
  }

  inline pos_3d root_tgeomanager::tgeonav::to_local(pos_3d master) const {
    //ROOT has 4 different classes for a vector of three doubles and they manage to use none of them.
    //This wins a nomination for most cringeworthy API, I wrote this function solely to avoid looking at it.
    //There are two pointer indirections for stuff that really will be in registers at point of call 99.9% of the time.
    //To make really sure it could not be inlined, the multiply is after two layers of function calls,
    //and then you look at the actual multiply code... vectorization? naah forget about it,
    //they test for identity and call memcpy lest the branch predictor get bored. Might have been a good idea in a Pentium II...
    //This is a textbook example of how to best fight the compiler attempts at producing efficient code, might as well write python.
    double m[3];
    master.GetCoordinates(m);
    double l[3];
    pos_3d ret;
    MasterToLocal(m, l);
    ret.SetCoordinates(l);
    return ret;
  }

  inline pos_3d root_tgeomanager::tgeonav::to_master(pos_3d local) const {
    double l[3];
    local.GetCoordinates(l);
    double m[3];
    pos_3d ret;
    LocalToMaster(l, m);
    ret.SetCoordinates(m);
    return ret;
  }

}

UFW_DECLARE_COMPLEX_DATA(sand::root_tgeomanager);
