#include "Geometry.hpp"
#include "Line.hpp"
#include "Polyline.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace Slic3r {

Linef3 transform(const Linef3& line, const Transform3d& t)
{
    typedef Eigen::Matrix<double, 3, 2> LineInMatrixForm;

    LineInMatrixForm world_line;
    ::memcpy((void*)world_line.col(0).data(), (const void*)line.a.data(), 3 * sizeof(double));
    ::memcpy((void*)world_line.col(1).data(), (const void*)line.b.data(), 3 * sizeof(double));

    LineInMatrixForm local_line = t * world_line.colwise().homogeneous();
    return Linef3(Vec3d(local_line(0, 0), local_line(1, 0), local_line(2, 0)), Vec3d(local_line(0, 1), local_line(1, 1), local_line(2, 1)));
}

bool Line::intersection_infinite(const Line &other, Point* point) const
{
    Vec2d a1 = this->a.cast<double>();
    Vec2d v12 = (other.a - this->a).cast<double>();
    Vec2d v1 = (this->b - this->a).cast<double>();
    Vec2d v2 = (other.b - other.a).cast<double>();
    double denom = cross2(v1, v2);
    if (std::fabs(denom) < EPSILON)
        return false;
    double t1 = cross2(v12, v2) / denom;
    *point = (a1 + t1 * v1).cast<coord_t>();
    return true;
}

// Distance to the closest point of line.
double Line::distance_to_squared(const Point &point, const Point &a, const Point &b)
{
    const Vec2d   v  = (b - a).cast<double>();
    const Vec2d   va = (point  - a).cast<double>();
    const double  l2 = v.squaredNorm();  // avoid a sqrt
    if (l2 == 0.0) 
        // a == b case
        return va.squaredNorm();
    // Consider the line extending the segment, parameterized as a + t (b - a).
    // We find projection of this point onto the line. 
    // It falls where t = [(this-a) . (b-a)] / |b-a|^2
    const double t = va.dot(v) / l2;
    if (t < 0.0)      return va.squaredNorm();  // beyond the 'a' end of the segment
    else if (t > 1.0) return (point - b).cast<double>().squaredNorm();  // beyond the 'b' end of the segment
    return (t * v - va).squaredNorm();
}

double Line::perp_distance_to(const Point &point) const
{
    const Line  &line = *this;
    const Vec2d  v  = (line.b - line.a).cast<double>();
    const Vec2d  va = (point - line.a).cast<double>();
    if (line.a == line.b)
        return va.norm();
    return std::abs(cross2(v, va)) / v.norm();
}

double Line::orientation() const
{
    double angle = this->atan2_();
    if (angle < 0) angle = 2*PI + angle;
    return angle;
}

double Line::direction() const
{
    double atan2 = this->atan2_();
    return (fabs(atan2 - PI) < EPSILON) ? 0
        : (atan2 < 0) ? (atan2 + PI)
        : atan2;
}

bool Line::parallel_to(double angle) const
{
    return Slic3r::Geometry::directions_parallel(this->direction(), angle);
}

bool Line::intersection(const Line &l2, Point *intersection) const
{
    const Line  &l1  = *this;
    const Vec2d  v1  = (l1.b - l1.a).cast<double>();
    const Vec2d  v2  = (l2.b - l2.a).cast<double>();
    const Vec2d  v12 = (l1.a - l2.a).cast<double>();
    double       denom  = cross2(v1, v2);
    double       nume_a = cross2(v2, v12);
    double       nume_b = cross2(v1, v12);
    if (fabs(denom) < EPSILON)
#if 0
        // Lines are collinear. Return true if they are coincident (overlappign).
        return ! (fabs(nume_a) < EPSILON && fabs(nume_b) < EPSILON);
#else
        return false;
#endif
    double t1 = nume_a / denom;
    double t2 = nume_b / denom;
    if (t1 >= 0 && t1 <= 1.0f && t2 >= 0 && t2 <= 1.0f) {
        // Get the intersection point.
        (*intersection) = (l1.a.cast<double>() + t1 * v1).cast<coord_t>();
        return true;
    }
    return false;  // not intersecting
}

Vec3d Linef3::intersect_plane(double z) const
{
    auto   v = (this->b - this->a).cast<double>();
    double t = (z - this->a(2)) / v(2);
    return Vec3d(this->a(0) + v(0) * t, this->a(1) + v(1) * t, z);
}

Point Line::point_at(double distance) const {
    Point point;
    double len = this->length();
    point = this->a;
    if (this->a.x() != this->b.x())
        point.x() = this->a.x() + (this->b.x() - this->a.x()) * distance / len;
    if (this->a.y() != this->b.y())
        point.y() = this->a.y() + (this->b.y() - this->a.y()) * distance / len;
    return point;
}

}
