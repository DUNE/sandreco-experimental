
#include <hdf5.hpp>

namespace sand::hdf5 {

  namespace {

    ufw::config::io io_parse(const ufw::config& cfg) {
      static const std::map<std::string, ufw::config::io> s_parse_map{
          {"read", ufw::config::read}, {"write", ufw::config::write}, {"overwrite", ufw::config::overwrite}};
      auto str = cfg.value("io", std::string("read"));
      if (auto it = s_parse_map.find(str); it != s_parse_map.end()) {
        return it->second;
      } else {
        UFW_ERROR("Invalid I/O operation '{}', values include 'read', 'write' and 'overwrite'", str);
      }
    }

    unsigned int acc_t(ufw::config::io io) {
      switch (io) {
      case ufw::config::read:
        return H5F_ACC_RDONLY;
      case ufw::config::overwrite:
        return H5F_ACC_TRUNC;
      case ufw::config::write:
      default:
        return H5F_ACC_RDWR;
      }
    }

  } // namespace

  size_t ndarray::ndrange::flat_size() const {
    return std::accumulate(begin(), end(), 1, [](auto lhs, auto rhs) { return lhs * rhs; });
  }

  ndarray::ndarray(const ufw::config& cfg) try
    : m_io_type(io_parse(cfg)), m_file(cfg.path_at("uri", m_io_type), acc_t(m_io_type)) {
    // Only support top level datasets
    H5::Group group = m_file.openGroup("/");
    for (hsize_t i = 0; i < group.getNumObjs(); ++i) {
      if (group.getObjTypeByIdx(i) == H5G_DATASET) {
        m_datasets.push_back(group.getObjnameByIdx(i));
      }
    }
  } catch (H5::Exception& error) {
    UFW_ERROR("HDF5 Error: {}", error.getDetailMsg());
  }

  std::string ndarray::attribute(const std::string& dataset, const std::string& attr) {
    H5::Attribute a = m_file.openDataSet(dataset).openAttribute(attr);
    size_t sz       = a.getSpace().getSimpleExtentNpoints();
    H5::StrType str(H5::PredType::C_S1, H5T_VARIABLE);
    std::string ret;
    a.read(str, ret);
    return ret;
  }

  void ndarray::set_attribute(const std::string& dataset, const std::string& attr, const std::string& value) {
    H5::DataSpace s = H5::DataSpace(H5S_SCALAR);
    H5::StrType str(H5::PredType::C_S1, H5T_VARIABLE);
    H5::Attribute a = m_file.openDataSet(dataset).createAttribute(attr, str, s);
    a.write(str, value);
  }

  ndarray::ndrange ndarray::range(const std::string& ds) const try {
    H5::DataSet dataset     = m_file.openDataSet(ds);
    H5::DataSpace dataspace = dataset.getSpace();
    ndrange ret(dataspace.getSimpleExtentNdims(), 0);
    dataspace.getSimpleExtentDims(ret.data());
    ret.set_type(dataset.getDataType());
    return ret;
  } catch (H5::Exception& error) {
    UFW_ERROR("HDF5 Error: {}", error.getDetailMsg());
  }

  void ndarray::read(const std::string& ds, void* ptr) {
    H5::DataSet dataset = m_file.openDataSet(ds);
    dataset.read(ptr, dataset.getDataType());
  }

  void ndarray::write(const std::string& ds, const ndrange& nd, const void* ptr) {
    H5::DataSpace dataspace(nd.size(), nd.data());
    H5::DataSet dataset = m_file.createDataSet(ds, nd.type(), dataspace);
    dataset.write(ptr, nd.type());
  }

} // namespace sand::hdf5
