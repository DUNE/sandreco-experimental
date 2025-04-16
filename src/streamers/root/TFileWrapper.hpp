#include <ufw/utils.hpp>

#include <TFile.h>

namespace sand::common::root {

  /**
   * @class TFileWrapper
   * @brief A wrapper around ROOT's TFile class.
   *
   * This class provides a more robust interface for TFile::Get with automatic lifetime management
   * of both the objects and the file itself.
   * 
   * @warning It is strongly recommended to use this wrapper for reading a TFile. It is not recommended for writing.
   * 
   * @warning The implementation assumes you will Get at least one object successfully. If you did not, you can destroy
   * this via Close(), otherwise risk leaking it.
   */
  class TFileWrapper : public TFile {

  public:
    using TFile::TFile;

    /**
     * @brief Retrieves an object from the associated ROOT file.
     *
     * To obtain an object from a TFile, use this method. It simply calls TFile::Get and wraps the object in a shared_ptr.
     * It correctly handles Get'ting the same object multiple times or different objects.
     *
     * @param objname The name of the object to retrieve.
     * @return A shared pointer to the retrieved object.
     * @throw std::runtime_error if no object with the specified name exists in the file.
     */

    template <typename T>
    std::shared_ptr<T> GetShared(const std::string& objname) {
      T* obj = TFile::Get<T>(objname.c_str());
      if (!obj) {
        UFW_ERROR("No object named '{}' in file.", objname);
      }
      auto it = m_objects.find(obj);
      if (it == m_objects.end()) {
        auto deleter = [this](T* obj) {
          auto it = m_objects.find(obj);
          if (it != m_objects.end()) {
            m_objects.erase(it);
          }
          delete obj;
          Close();
        };
        auto ptr = std::shared_ptr<T>(obj, deleter);
        m_objects.emplace(obj, ptr);
        return ptr;
      }
      return std::static_pointer_cast<T>(it->second.lock());
    }

    /**
     * Intercepts an explicit call to Close(). Do not use the object afterwards.
     * 
     * @note delete this is usually frowned upon for good reason, however this object manages its own lifetime,
     * and it should never be destroyed manually by the user.
     */
    void Close() {
      if (m_objects.empty())
        delete this;
    }

  private:
    /**
     * A TFileWrapper is destroyed automatically when the last object retrieved from it goes out of scope,
     * do not delete it yourself.
     */
    ~TFileWrapper() {
      UFW_INFO("Destroying TFileWrapper for file {} at {}", GetPath(), fmt::ptr(this));
      assert(m_objects.empty());
      TFile::Close();
    }

  private:
    std::map<TObject*, std::weak_ptr<TObject>> m_objects;

  };

}
