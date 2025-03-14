#ifndef SDB_STOPPOINT_COLLECTION_HPP
#define SDB_STOPPOINT_COLLECTION_HPP

#include <vector>
#include <memory>
#include <libsdb/types.hpp>
#include <algorithm>

// covers breakpoint sites, source-level breakpoints and watchpoints
// essentially a wrapper class around a collection of stopping points 
namespace sdb {
    template <typename Stoppoint>
    class stoppoint_collection {
        public:
            /* adds a new stop point to collection */
            Stoppoint& push(std::unique_ptr<Stoppoint> bs); //bs - breakpoint sites

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

        private:
            using points_t = std::vector<std::unique_ptr<Stoppoint>>;
            points_t stoppoints_;

            /* returns iterator to the stop point's vector */
            typename points_t::iterator find_by_id(typename Stoppoint::id_type id);
            typename points_t::const_iterator find_by_id(typename Stoppoint::id_type id) const;
            typename points_t::iterator find_by_address(sdb::virt_addr address);
            typename points_t::const_iterator find_by_address(sdb::virt_addr address) const;
    };

    template <typename Stoppoint>
    Stoppoint& stoppoint_collection<Stoppoint>::push(std::unique_ptr<Stoppoint> bs) {
        stoppoints_.push_back(std::move(bs));
        return *stoppoints_.back();
    }
}

#endif