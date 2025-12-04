
#include <hdf5.hpp>

namespace sand::hdf5 {

  size_t ndarray::ndrange::flat_size() const {
    return std::accumulate(begin(), end(), 1, [](auto lhs, auto rhs) { return lhs * rhs; });
  }

  ndarray::ndarray(const ufw::config& cfg) try : m_file(cfg.path_at("uri"), H5F_ACC_RDONLY) {
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

  ndarray::ndrange ndarray::range(const std::string& ds) const try {
    H5::DataSet dataset     = m_file.openDataSet(ds);
    H5::DataSpace dataspace = dataset.getSpace();
    ndrange ret(dataspace.getSimpleExtentNdims(), 0);
    dataspace.getSimpleExtentDims(ret.data());
    ret.set_type(dataset.getDataType().getClass());
    return ret;
  } catch (H5::Exception& error) {
    UFW_ERROR("HDF5 Error: {}", error.getDetailMsg());
  }

  void ndarray::read(const std::string& ds, void* ptr) {
    H5::DataSet dataset = m_file.openDataSet(ds);
    dataset.read(ptr, dataset.getDataType());
  }

  /*  void ndarray::write(const std::string& ds, const void* ptr) {
      H5::DataSet dataset = m_file.openDataSet(ds);
      dataset.write(ptr, dataset.getDataType());
    }*/

} // namespace sand::hdf5
