#pragma once

#include <edep_reader/EDEPTrajectory.h>

// Binary search not implemented due to low counts of child elements

/**
 * @class EDEPTree
 * @brief Represents a tree structure of trajectories.
 *
 * This class extends EDEPTrajectory to organize trajectories in a tree structure.
 * It provides iterators for traversing the tree, as well as functions for adding,
 * removing, and manipulating trajectories within the tree.
 */
class EDEPTree : public EDEPTrajectory {
    
  public:
    // Iterators
    typedef std::vector<EDEPTrajectory>::iterator child_iterator;

    /**
     * @class iterator
     * @brief Iterator for traversing the tree.
     */
    class iterator {

      public:
        typedef EDEPTrajectory value_type;
        typedef std::forward_iterator_tag iterator_category;
        typedef ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;

        iterator() = default;

        reference operator *  () {return *current_it_;};
        pointer operator -> () {return &(*current_it_);};
        bool operator == (const iterator& it) {return (this->parent_trj_ == it.parent_trj_ && this->current_it_ == it.current_it_);};
        bool operator != (const iterator& it) {return (this->parent_trj_ != it.parent_trj_ || this->current_it_ != it.current_it_);};
        iterator& operator ++ (); //++it
        iterator  operator ++ (int) {iterator tmpIt = *this; ++*this; return tmpIt;}; //it++
        // iterator& operator -- (); //--it
        // iterator  operator -- (int); //it--

      private:
        iterator(pointer parent_trj_, child_iterator child_it);

        pointer parent_trj_ = nullptr;
        child_iterator current_it_;

      friend class EDEPTree;
    };
    
    typedef std::vector<EDEPTrajectory>::const_iterator const_child_iterator;

    /**
     * @class const_iterator
     * @brief Const iterator for traversing the tree.
     */
    class const_iterator {

      public:
        typedef const EDEPTrajectory value_type;
        typedef std::forward_iterator_tag iterator_category;
        typedef ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;
      
        const_iterator() = default;
        reference operator *  () {return *current_it_;};
        pointer operator -> () {return &(*current_it_);};
        bool operator == (const const_iterator& it) {return (this->parent_trj_ == it.parent_trj_ && this->current_it_ == it.current_it_);};
        bool operator != (const const_iterator& it) {return (this->parent_trj_ != it.parent_trj_ || this->current_it_ != it.current_it_);};
        const_iterator& operator ++ (); //++it
        const_iterator  operator ++ (int) {const_iterator tmpIt = *this; ++*this; return tmpIt;}; //it++
        // const_iterator& operator -- (); //--it
        // const_iterator  operator -- (int); //it--

      private:
        const_iterator(pointer parent_trj_, const_child_iterator child_it);

        pointer parent_trj_ = nullptr;
        const_child_iterator current_it_;

      friend class EDEPTree;
    };

    // Constructor
    EDEPTree();

    // Functions
          iterator begin()       {return iterator(nullptr, this->GetChildrenTrajectories().begin());}
    const_iterator begin() const {return const_iterator(nullptr, this->GetChildrenTrajectories().begin());}
          iterator end()         {return iterator(nullptr, this->GetChildrenTrajectories().end());}
    const_iterator end()   const {return const_iterator(nullptr, this->GetChildrenTrajectories().end());}


    void InizializeFromEdep(const TG4Event& edep_event);
    void InizializeFromTrj(const std::vector<EDEPTrajectory>& trajectories_vect);
        
    void AddTrajectory  (const EDEPTrajectory& trajectory);
    void AddTrajectoryTo(const EDEPTrajectory& trajectory, iterator it);
    
    void RemoveTrajectory    (int trj_id);
    void RemoveTrajectoryFrom(int trj_id, iterator it);
    void MoveTrajectoryTo    (int id_to_move, int next_parent_id);

    bool HasTrajectory (int trj_id) const;
    bool IsTrajectoryIn(int trj_id, iterator it);
    bool IsTrajectoryIn(int trj_id, const_iterator it) const;
    
          iterator GetTrajectory(int trj_id)       {return std::find_if(this->begin(), this->end(), [trj_id](const EDEPTrajectory& trj){ return trj_id == trj.GetId();});}
    const_iterator GetTrajectory(int trj_id) const {return std::find_if(this->begin(), this->end(), [trj_id](const EDEPTrajectory& trj){ return trj_id == trj.GetId();});}
          
          iterator GetParentOf(int trj_id);
    const_iterator GetParentOf(int trj_id) const;
          iterator GetParentOf(int trj_id, iterator it);
    const_iterator GetParentOf(int trj_id, const_iterator it) const;

          iterator GetTrajectoryFrom(int trj_id, iterator it);
    const_iterator GetTrajectoryFrom(int trj_id, const_iterator it) const;

          iterator GetTrajectoryEnd(iterator start);
    const_iterator GetTrajectoryEnd(const_iterator start) const;

          iterator  GetTrajectoryWithHitId(int id);
    const_iterator  GetTrajectoryWithHitId(int id) const;

          iterator  GetTrajectoryWithHitIdInDetector(int id, component component_name);
    const_iterator  GetTrajectoryWithHitIdInDetector(int id, component component_name) const;

    /**
     * @brief Filter trajectories based on a custom predicate and copy the results to an output iterator.
     *
     * This function iterates over all trajectories within the tree and applies a custom predicate function
     * to each trajectory. Trajectories that satisfy the filtering condition specified by the predicate
     * function are copied to the provided output iterator.
     *
     * @tparam OutputIterator The type of the output iterator where the filtered trajectories will be copied.
     * @tparam F The type of the predicate function used for filtering trajectories.
     *
     * @param out_it The output iterator where the filtered trajectories will be copied.
     * @param funct The predicate function to filter trajectories. It should accept an EDEPTrajectory object
     *              as input and return a boolean value indicating whether the trajectory satisfies the filtering condition.
     *
     * @return The output iterator after copying the filtered trajectories. It points to the position after the last
     *         copied trajectory in the output container.
     *
     * @note The predicate function should have the signature `bool funct(const EDEPTrajectory&)`.
     *       It should return true if the given trajectory satisfies the filtering condition, and false otherwise.
     *
     * @warning The behavior of this function is undefined if the output iterator is not compatible with
     *          the type of trajectories being copied.
     *
     * @see EDEPTree
     */
    template <typename OutputIterator, typename F>
    OutputIterator Filter(OutputIterator out_it, F&& funct) {
            for (auto first = this->begin(); first != this->end(); ++first)
            {
                if (std::forward<F>(funct)(*first))
                {
                    *out_it = *first;
                    ++out_it;
                }
            }
        
            return out_it;
        
    }

  private:  
    void CreateTree(const std::vector<EDEPTrajectory>& trajectories_vect);
};

