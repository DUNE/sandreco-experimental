
#pragma once

#include <memory>
#include <type_traits>

namespace sand::utils {

  template <typename T>
  class list;

  template <typename T>
  class idl_node {
    friend class list<T>;
    void splice_this_after(T* other) {
      m_next         = other->m_next;
      other->m_next  = this;
      m_next->m_prev = this;
      m_prev         = other;
    }
    void splice_this_before(T* other) {
      m_prev         = other->m_prev;
      other->m_prev  = this;
      m_prev->m_next = this;
      m_next         = other;
    }
    T* pop() {
      m_prev->m_next = m_next;
      m_next->m_prev = m_prev;
      m_next = m_prev = nullptr;
      return this;
    }
    T* m_next           = nullptr;
    T* m_prev           = nullptr;
    virtual ~idl_node() = default;
  };

  template <typename T>
  class list {
    static_assert(std::is_base_of_v<idl_node<T>, T> && std::is_convertible_v<T*, idl_node<T>*>,
                  "T must inherit publicly from idl_node<T>");

   public:
    struct const_iterator {
     public:
      const_iterator(const const_iterator&)             = default;
      const_iterator& operator= (const const_iterator&) = default;

      const_iterator& operator++ () {
        m_node = m_node->m_next;
        return *this;
      };

      const_iterator& operator-- () {
        m_node = m_node->m_prev;
        return *this;
      };

      const_iterator operator++ (int) {
        auto* n = m_node;
        m_node  = m_node->m_next;
        return const_iterator{n};
      };

      const_iterator operator-- (int) {
        auto* n = m_node;
        m_node  = m_node->m_prev;
        return const_iterator{n};
      };

      const T& operator* () const { return *m_node; }

      const T* operator->() const { return m_node; }

     private:
      friend class list<T>;
      friend bool operator== (const_iterator, const_iterator);
      const_iterator(const T* n) : m_node(n) {}

     private:
      const T* m_node;
    };

    struct iterator {
     public:
      iterator(const iterator&)             = default;
      iterator& operator= (const iterator&) = default;

      iterator& operator++ () {
        m_node = m_node->m_next;
        return *this;
      };

      iterator& operator-- () {
        m_node = m_node->m_prev;
        return *this;
      };

      iterator operator++ (int) {
        auto* n = m_node;
        m_node  = m_node->m_next;
        return iterator{n};
      };

      iterator operator-- (int) {
        auto* n = m_node;
        m_node  = m_node->m_prev;
        return iterator{n};
      };

      T& operator* () const { return *m_node; }

      T& operator* () { return *m_node; }

      T* operator->() const { return m_node; }

      T* operator->() { return m_node; }

      operator const_iterator() const { return const_iterator{m_node}; }

     private:
      friend class list<T>;
      friend bool operator== (iterator, iterator);
      iterator(T* n) : m_node(n) {}

     private:
      T* m_node;
    };

   public:
    list() { m_node_head.m_next = m_node_head.m_prev = static_cast<T*>(&m_node_head); }

    list(const_iterator first, const_iterator last) { insert(begin(), first, last); }

    iterator begin() { return iterator{m_node_head->m_next}; }

    iterator end() { return iterator{static_cast<T*>(&m_node_head)}; }

    const_iterator begin() const { return const_iterator{m_node_head->m_next}; }

    const_iterator end() const { return const_iterator{static_cast<T*>(&m_node_head)}; }

    const_iterator cbegin() const { return const_iterator{m_node_head->m_next}; }

    const_iterator cend() const { return const_iterator{static_cast<T*>(&m_node_head)}; }

    template <typename U = T>
    std::unique_ptr<U> pop_back() {
      return m_node_head->m_prev->pop();
    }

    template <typename U = T>
    std::unique_ptr<U> pop_front() {
      return m_node_head->m_next->pop();
    }

    void erase(const_iterator it) { delete it->m_node->pop(); }

    void erase(const_iterator first, const_iterator last) {
      while (first != last) {
        erase(first++);
      }
    }

    template <typename U>
    std::enable_if_t<std::is_base_of_v<T, U>, void> insert(const_iterator after, std::unique_ptr<U>&& node) {
      node->release()->splice_this_after(after.m_node);
    }

    template <typename U>
    std::enable_if_t<std::is_base_of_v<T, U>, void> insert(const_iterator after, const_iterator first,
                                                           const_iterator last) {
      first->m_node->m_prev->m_next = last->m_node;
      last->m_node->m_prev          = first->m_node;
      auto* beforenode              = after->m_node->m_next;
      after->m_node->m_next         = first->m_node;
      first->m_node->m_prev         = after->m_node;
      last->m_node->m_prev->m_next  = beforenode;
      beforenode->m_prev            = last->m_node->m_prev;
    }

    template <typename U>
    std::enable_if_t<std::is_base_of_v<T, U>, void> push_back(std::unique_ptr<U>&& node) {
      node->release()->splice_this_before(static_cast<T*>(&m_node_head));
    }

    template <typename U>
    std::enable_if_t<std::is_base_of_v<T, U>, void> push_front(std::unique_ptr<U>&& node) {
      node->release()->splice_this_after(static_cast<T*>(&m_node_head));
    }

    template <typename U, typename... Args>
    std::enable_if_t<std::is_base_of_v<T, U>, void> emplace(const_iterator after, Args&&... args) {
      auto* node = new U(std::forward<Args>(args)...);
      node->splice_this_after(after.m_node);
    }

    template <typename U, typename... Args>
    std::enable_if_t<std::is_base_of_v<T, U>, void> emplace_back(Args&&... args) {
      auto* node = new U(std::forward<Args>(args)...);
      node->splice_this_before(static_cast<T*>(&m_node_head));
    }

    template <typename U, typename... Args>
    std::enable_if_t<std::is_base_of_v<T, U>, void> emplace_front(Args&&... args) {
      auto* node = new U(std::forward<Args>(args)...);
      node->splice_this_after(static_cast<T*>(&m_node_head));
    }

    size_t size() const { return std::distance(begin(), end()); }

    bool empty() const { return begin() != end(); }

   private:
    idl_node<T> m_node_head;
  };

  template <typename T>
  bool operator== (typename list<T>::const_iterator lhs, typename list<T>::const_iterator rhs) {
    return lhs.m_node == rhs.m_node;
  }

  template <typename T>
  bool operator!= (typename list<T>::const_iterator lhs, typename list<T>::const_iterator rhs) {
    return !(lhs == rhs);
  }

  template <typename T>
  bool operator== (typename list<T>::iterator lhs, typename list<T>::iterator rhs) {
    return lhs.m_node == rhs.m_node;
  }

  template <typename T>
  bool operator!= (typename list<T>::iterator lhs, typename list<T>::iterator rhs) {
    return !(lhs == rhs);
  }

} // namespace sand::utils
