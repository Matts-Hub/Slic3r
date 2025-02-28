#ifndef slic3r_ExtrusionEntityCollection_hpp_
#define slic3r_ExtrusionEntityCollection_hpp_

#include "libslic3r.h"
#include "ExtrusionEntity.hpp"

namespace Slic3r {

class ExtrusionEntityCollection : public ExtrusionEntity
{
public:
    ExtrusionEntityCollection* clone() const;

    /// Owned ExtrusionEntities and descendent ExtrusionEntityCollections.
    /// Iterating over this needs to check each child to see if it, too is a collection.
    ExtrusionEntitiesPtr entities;     // we own these entities

    std::vector<size_t> orig_indices;  // handy for XS
    bool no_sort;

    ExtrusionEntityCollection(): no_sort(false) {};
    ExtrusionEntityCollection(const ExtrusionEntityCollection &other) : orig_indices(other.orig_indices), no_sort(other.no_sort) { this->append(other.entities); }
    ExtrusionEntityCollection(ExtrusionEntityCollection &&other) : entities(std::move(other.entities)), orig_indices(std::move(other.orig_indices)), no_sort(other.no_sort) {}
    explicit ExtrusionEntityCollection(const ExtrusionPaths &paths);
    ExtrusionEntityCollection& operator=(const ExtrusionEntityCollection &other);
    ExtrusionEntityCollection& operator=(ExtrusionEntityCollection &&other) 
        { this->entities = std::move(other.entities); this->orig_indices = std::move(other.orig_indices); this->no_sort = other.no_sort; return *this; }
    ~ExtrusionEntityCollection() { clear(); }

    /// Operator to convert and flatten this collection to a single vector of ExtrusionPaths.
    explicit operator ExtrusionPaths() const;
    
    bool is_collection() const { return true; };
    ExtrusionRole role() const override {
        ExtrusionRole out = erNone;
        for (const ExtrusionEntity *ee : entities) {
            ExtrusionRole er = ee->role();
            out = (out == erNone || out == er) ? er : erMixed;
        }
        return out;
    }
    bool can_reverse() const { return !this->no_sort; };
    bool empty() const { return this->entities.empty(); };
    void clear();
    void swap (ExtrusionEntityCollection &c);
    void append(const ExtrusionEntity &entity) { this->entities.push_back(entity.clone()); }
    void append(const ExtrusionEntitiesPtr &entities) { 
        this->entities.reserve(this->entities.size() + entities.size());
        for (ExtrusionEntitiesPtr::const_iterator ptr = entities.begin(); ptr != entities.end(); ++ptr)
            this->entities.push_back((*ptr)->clone());
    }
    void append(ExtrusionEntitiesPtr &&src) {
        if (entities.empty())
            entities = std::move(src);
        else {
            std::move(std::begin(src), std::end(src), std::back_inserter(entities));
            src.clear();
        }
    }
    void append(const ExtrusionPaths &paths) {
        this->entities.reserve(this->entities.size() + paths.size());
        for (const ExtrusionPath &path : paths)
            this->entities.emplace_back(path.clone());
    }
    void append(ExtrusionPaths &&paths) {
        this->entities.reserve(this->entities.size() + paths.size());
        for (ExtrusionPath &path : paths)
            this->entities.emplace_back(new ExtrusionPath(std::move(path)));
    }
    void replace(size_t i, const ExtrusionEntity &entity);
    void remove(size_t i);
    ExtrusionEntityCollection chained_path(bool no_reverse = false, ExtrusionRole role = erMixed) const;
    void chained_path(ExtrusionEntityCollection* retval, bool no_reverse = false, ExtrusionRole role = erMixed, std::vector<size_t>* orig_indices = nullptr) const;
    ExtrusionEntityCollection chained_path_from(Point start_near, bool no_reverse = false, ExtrusionRole role = erMixed) const;
    void chained_path_from(Point start_near, ExtrusionEntityCollection* retval, bool no_reverse = false, ExtrusionRole role = erMixed, std::vector<size_t>* orig_indices = nullptr) const;
    void reverse();
    Point first_point() const { return this->entities.front()->first_point(); }
    Point last_point() const { return this->entities.back()->last_point(); }
    // Produce a list of 2D polygons covered by the extruded paths, offsetted by the extrusion width.
    // Increase the offset by scaled_epsilon to achieve an overlap, so a union will produce no gaps.
    void polygons_covered_by_width(Polygons &out, const float scaled_epsilon) const override;
    // Produce a list of 2D polygons covered by the extruded paths, offsetted by the extrusion spacing.
    // Increase the offset by scaled_epsilon to achieve an overlap, so a union will produce no gaps.
    // Useful to calculate area of an infill, which has been really filled in by a 100% rectilinear infill.
    void polygons_covered_by_spacing(Polygons &out, const float scaled_epsilon) const override;
    Polygons polygons_covered_by_width(const float scaled_epsilon = 0.f) const
        { Polygons out; this->polygons_covered_by_width(out, scaled_epsilon); return out; }
    Polygons polygons_covered_by_spacing(const float scaled_epsilon = 0.f) const
        { Polygons out; this->polygons_covered_by_spacing(out, scaled_epsilon); return out; }

    /// Recursively count paths and loops contained in this collection 
    size_t items_count() const;

    /// Returns a single vector of pointers to all non-collection items contained in this one
    /// \param retval a pointer to the output memory space.
    /// \param preserve_ordering Flag to method that will flatten if and only if the underlying collection is sortable when True (default: False).
    void flatten(ExtrusionEntityCollection* retval, bool preserve_ordering = false) const;

    /// Returns a flattened copy of this ExtrusionEntityCollection. That is, all of the items in its entities vector are not collections.
    /// You should be iterating over flatten().entities if you are interested in the underlying ExtrusionEntities (and don't care about hierarchy).
    /// \param preserve_ordering Flag to method that will flatten if and only if the underlying collection is sortable when True (default: False).
    ExtrusionEntityCollection flatten(bool preserve_ordering = false) const;

    double min_mm3_per_mm() const;
    double total_volume() const override { double volume=0.; for (const auto& ent : entities) volume+=ent->total_volume(); return volume; }

    // Following methods shall never be called on an ExtrusionEntityCollection.
    Polyline as_polyline() const {
        throw std::runtime_error("Calling as_polyline() on a ExtrusionEntityCollection");
        return Polyline();
    };

    void collect_polylines(Polylines &dst) const override {
        for (ExtrusionEntity* extrusion_entity : this->entities)
            extrusion_entity->collect_polylines(dst);
    }

    double length() const override {
        throw std::runtime_error("Calling length() on a ExtrusionEntityCollection");
        return 0.;        
    }
    virtual void visit(ExtrusionVisitor &visitor) { visitor.use(*this); };
    virtual void visit(ExtrusionVisitorConst &visitor) const { visitor.use(*this); };
};

//// visitors /////

class CountEntities : public ExtrusionVisitorConst {
public:
    size_t count(const ExtrusionEntity &coll) { coll.visit(*this); return leaf_number; }
    size_t leaf_number = 0;
    virtual void default_use(const ExtrusionEntity &entity) override { ++leaf_number; }
    virtual void use(const ExtrusionEntityCollection &coll) override;
};

class FlatenEntities : public ExtrusionVisitorConst {
    ExtrusionEntityCollection to_fill;
    bool preserve_ordering;
public:
    FlatenEntities(bool preserve_ordering) : preserve_ordering(preserve_ordering) {}
    FlatenEntities(ExtrusionEntityCollection pattern, bool preserve_ordering) : preserve_ordering(preserve_ordering) {
        to_fill.no_sort = pattern.no_sort;
        to_fill.orig_indices = pattern.orig_indices;
    }
    ExtrusionEntityCollection get() {
        return to_fill;
    };
    ExtrusionEntityCollection&& flatten(const ExtrusionEntityCollection &to_flatten) &&;
    virtual void default_use(const ExtrusionEntity &entity) override { to_fill.append(entity); }
    virtual void use(const ExtrusionEntityCollection &coll) override;
};

}

#endif
