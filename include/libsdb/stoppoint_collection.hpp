#ifndef SDB_STOPPOINT_COLLECTION_HPP
#define SDB_STOPPOINT_COLLECTION_HPP

#include <vector>
#include <memory>
#include <libsdb/types.hpp>
#include <algorithm>
#include <libsdb/error.hpp>

// covers breakpoint sites, source-level breakpoints and watchpoints
// essentially a wrapper class around a collection (container) of stopping points 
// generalize it to avoid additional code
namespace sdb {
    template <typename Stoppoint>
    class stoppoint_collection {
        public:
            /* Adds a new stop point to collection 
            * @param bs breakpoint to add
            * Return a reference to the added stoppoint
            */
            Stoppoint& push(std::unique_ptr<Stoppoint> bs); 

            /* check if the collection contains a matching stop point */
            bool contains_id(typename Stoppoint::id_type id) const;
            bool contains_address(virt_addr address) const;

            /* if stoppoint is enabled at said address */
            bool enabled_stoppoint_at_address(virt_addr address) const;

            /* get functions */
            Stoppoint& get_by_id(typename Stoppoint::id_type id);
            const Stoppoint& get_by_id(typename Stoppoint::id_type id) const;
            Stoppoint& get_by_address(virt_addr address);
            const Stoppoint& get_by_address(virt_addr address) const;

            /* remove stop points */
            void remove_by_id(typename Stoppoint::id_type id);
            void remove_by_address(virt_addr address);

            /* users pass a function or lambda to be called with a reference each breakpoint site */
            template <typename F>
            void for_each(F f);
            template <typename F>
            void for_each(F f) const;

            std::size_t size() const { return stoppoints_.size(); }
            bool empty() const { return stoppoints_.empty(); }

            /* get all sites in a region of memory */
            std::vector<Stoppoint*> get_in_region(virt_addr low, virt_addr high) const;

        private:
            using points_t = std::vector<std::unique_ptr<Stoppoint>>;
            points_t stoppoints_;

            /* returns iterator to the stop point's vector */
            typename points_t::iterator find_by_id(typename Stoppoint::id_type id);
            typename points_t::const_iterator find_by_id(typename Stoppoint::id_type id) const;
            typename points_t::iterator find_by_address(sdb::virt_addr address);
            typename points_t::const_iterator find_by_address(sdb::virt_addr address) const;
    };

    /* function implementations */
    template <typename Stoppoint>
    Stoppoint& stoppoint_collection<Stoppoint>::push(std::unique_ptr<Stoppoint> bs) {
        //vector. cannot copy unique_ptr so we need to use std::move
        stoppoints_.push_back(std::move(bs));
        return *stoppoints_.back();
    }

    //points_t::iterator is a member of type of sdb::stoppoint_collection<Stoppoint>
    //avoid this by making the return type auto
    template<typename Stoppoint>
    auto stoppoint_collection<Stoppoint>::find_by_id(typename Stoppoint::id_type id) -> typename points_t::iterator {
        return std::find_if(begin(stoppoints_), end (stoppoints_), [=] (auto &point) { return point->id() == id; });
    }

    template<typename Stoppoint>
    auto stoppoint_collection<Stoppoint>::find_by_id(typename Stoppoint::id_type id) const -> typename points_t::const_iterator {
        //remove the const of this and call the non-const overload
        //still converts iterator to const_iterator so not in danger of breaking const_correctness
        return const_cast<stoppoint_collection*>(this)->find_by_id(id);
    }

    /* returns an iterator to the satisfied element or last */
    template<typename Stoppoint>
    auto stoppoint_collection<Stoppoint>::find_by_address(virt_addr address) 
        -> typename points_t::iterator {
        return std::find_if(begin(stoppoints_), end(stoppoints_), [=](auto& point) { return point->at_address(address);});
    }

    template<typename Stoppoint>
    auto stoppoint_collection<Stoppoint>::find_by_address(virt_addr address) const
        -> typename points_t::const_iterator {
        //iterator converted to const::iterator
        return const_cast<stoppoint_collection*>(this)->find_by_address(address); 
    }

    /* check if there's a stoppoint by this id in the collection */
    template<typename Stoppoint>
    bool stoppoint_collection<Stoppoint>::contains_id(typename Stoppoint::id_type id) const {
        return find_by_id(id) != end(stoppoints_);
    }

    template<typename Stoppoint>
    bool stoppoint_collection<Stoppoint>::contains_address(virt_addr address) const {
        return find_by_address(address) != end(stoppoints_);
    }

    template<typename Stoppoint>
    bool stoppoint_collection<Stoppoint>::enabled_stoppoint_at_address(
        virt_addr address) const {
        return contains_address(address) and get_by_address(address).is_enabled();
    }

    /***** GET FUNCTIONS *****/
    /* get_by_id overloads */
    template<typename Stoppoint>
    Stoppoint& stoppoint_collection<Stoppoint>::get_by_id(
        typename Stoppoint::id_type id) {
        auto it = find_by_id(id);
        if (it == end(stoppoints_)) {
            error::send("Invalid stoppoint id");
        }

        return **it; //one for the iterator, and one for the std::unique_ptr it references
    }

    /* same as above, remove const casting and call non-const overload */
    template<typename Stoppoint>
    const Stoppoint& stoppoint_collection<Stoppoint>::get_by_id(
        typename Stoppoint::id_type id) const {
        return const_cast<stoppoint_collection*>(this)->get_by_id(id);
    }

    template<typename Stoppoint>
    Stoppoint& stoppoint_collection<Stoppoint>::get_by_address(virt_addr address) {
        auto it = find_by_address(address);
        if (it == end(stoppoints_)) {
            error::send("Stoppoint with given address not found");
        }
        return **it;
    }

    template<typename Stoppoint>
    const Stoppoint& stoppoint_collection<Stoppoint>::get_by_address(virt_addr address) const {
        return const_cast<stoppoint_collection*>(this)->get_by_address(address);
    }

    /* find the relevant stop point, disable it then erase them from the container */
    template<typename Stoppoint>
    void stoppoint_collection<Stoppoint>::remove_by_id(typename Stoppoint::id_type id) {
        auto it = find_by_id(id);
        (**it).disable();
        stoppoints_.erase(it);
    }

    template<typename Stoppoint>
    void stoppoint_collection<Stoppoint>::remove_by_address(virt_addr address) {
        auto it = find_by_address(address);
        (**it).disable();
        stoppoints_.erase(it);
    }
    
    /* loops over the stop points in the collection, calling f parameter with each one */
    template<typename Stoppoint>
    template<typename F>
    void stoppoint_collection<Stoppoint>::for_each(F f) {
        for (auto& point : stoppoints_) {
            f(*point);
        }
    }

    /* call once for every element in the function */
    template<typename Stoppoint>
    template<typename F>
    void stoppoint_collection<Stoppoint>::for_each(F f) const {
        /* don't break const-correctness and pass a non-const stop point */
        for (const auto & point : stoppoints_) {
            /* we pass a lambda, then pass */
            f(*point);
        }
    }
    template<typename Stoppoint>
    std::vector<Stoppoint*> stoppoint_collection<Stoppoint>::get_in_region(virt_addr low, virt_addr high) const {
        std::vector<Stoppoint*> ret;
        //get each pointer out
        for (const auto & point : stoppoints_) {
            if (point->in_range(low, high)) {

                //extract the raw pointer from unique_ptr and push into vector 
                ret.push_back(point.get());
            }
        }
        return ret;
    }

}

#endif