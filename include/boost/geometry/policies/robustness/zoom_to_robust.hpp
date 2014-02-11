// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2013 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2013 Bruno Lalande, Paris, France.
// Copyright (c) 2013 Mateusz Loskot, London, UK.
// Copyright (c) 2013 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_ZOOM_TO_ROBUST_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_ZOOM_TO_ROBUST_HPP


#include <cstddef>

#include <boost/type_traits.hpp>

#include <boost/geometry/algorithms/envelope.hpp>
#include <boost/geometry/algorithms/expand.hpp>
#include <boost/geometry/algorithms/detail/recalculate.hpp>

#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/geometries/point.hpp>

#include <boost/geometry/policies/robustness/no_rescale_policy.hpp>
#include <boost/geometry/policies/robustness/segment_ratio_type.hpp>
#include <boost/geometry/policies/robustness/robust_point_type.hpp>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace zoom_to_robust
{

template <typename Box, std::size_t Dimension>
struct get_max_size
{
    static inline typename coordinate_type<Box>::type apply(Box const& box)
    {
        typename coordinate_type<Box>::type s
            = geometry::math::abs(geometry::get<1, Dimension>(box) - geometry::get<0, Dimension>(box));

        return (std::max)(s, get_max_size<Box, Dimension - 1>::apply(box));
    }
};

template <typename Box>
struct get_max_size<Box, 0>
{
    static inline typename coordinate_type<Box>::type apply(Box const& box)
    {
        return geometry::math::abs(geometry::get<1, 0>(box) - geometry::get<0, 0>(box));
    }
};

template <typename FpPoint, typename IntPoint, typename CalculationType>
struct rescale_strategy
{
    typedef typename geometry::coordinate_type<IntPoint>::type output_ct;

    rescale_strategy(FpPoint const& fp_min, IntPoint const& int_min, CalculationType const& the_factor)
        : m_fp_min(fp_min)
        , m_int_min(int_min)
        , m_multiplier(the_factor)
    {
    }

    template <std::size_t Dimension, typename Value>
    inline output_ct apply(Value const& value) const
    {
        // a + (v-b)*f
        CalculationType const a = static_cast<CalculationType>(get<Dimension>(m_int_min));
        CalculationType const b = static_cast<CalculationType>(get<Dimension>(m_fp_min));
        CalculationType const result = a + (value - b) * m_multiplier;
        return static_cast<output_ct>(result);
    }

    FpPoint m_fp_min;
    IntPoint m_int_min;
    CalculationType m_multiplier;
};

}} // namespace detail::zoom_to_robust
#endif // DOXYGEN_NO_DETAIL


// Define the IntPoint as a robust-point type
template <typename Point, typename FpPoint, typename IntPoint, typename CalculationType>
struct robust_point_type<Point, detail::zoom_to_robust::rescale_strategy<FpPoint, IntPoint, CalculationType> >
{
    typedef IntPoint type;
};

// Meta function for rescaling, if rescaling is done segment_ratio is based on long long
template <typename Point, typename FpPoint, typename IntPoint, typename CalculationType>
struct segment_ratio_type<Point, detail::zoom_to_robust::rescale_strategy<FpPoint, IntPoint, CalculationType> >
{
    typedef segment_ratio<boost::long_long_type> type;
};



template <typename Box>
inline typename coordinate_type<Box>::type get_max_size(Box const& box)
{
    return detail::zoom_to_robust::get_max_size<Box, dimension<Box>::value - 1>::apply(box);
}

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template <typename CoordinateType, typename IsFloatingPoint>
struct robust_type
{
};

template <typename CoordinateType>
struct robust_type<CoordinateType, boost::false_type>
{
    typedef CoordinateType type;
};

template <typename CoordinateType>
struct robust_type<CoordinateType, boost::true_type>
{
    typedef boost::long_long_type type;
};


template <typename IsFloatingPoint>
struct zoom_to_robust
{
    template
    <
        typename Geometry1, typename Geometry2, typename Geometry3,
        typename Geometry4, typename Geometry5, typename Geometry6,
        typename GeometryOut
    >
    static inline void apply(Geometry1 const& g1, Geometry2 const& g2, Geometry3 const& g3,
          Geometry4 const& g4, Geometry5 const& g5, Geometry6 const& g6,
          GeometryOut& og1, GeometryOut& og2, GeometryOut& og3,
          GeometryOut& og4, GeometryOut& og5, GeometryOut& og6)
    {
        // By default, just convert these geometries (until now: points or maybe segments)
        geometry::convert(g1, og1);
        geometry::convert(g2, og2);
        geometry::convert(g3, og3);
        geometry::convert(g4, og4);
        geometry::convert(g5, og5);
        geometry::convert(g6, og6);
    }
};

template <>
struct zoom_to_robust<boost::true_type>
{
    template
    <
        typename Geometry1, typename Geometry2, typename Geometry3,
        typename Geometry4, typename Geometry5, typename Geometry6,
        typename GeometryOut
    >
    static inline void apply(Geometry1 const& g1, Geometry2 const& g2, Geometry3 const& g3,
          Geometry4 const& g4, Geometry5 const& g5, Geometry6 const& g6,
          GeometryOut& og1, GeometryOut& og2, GeometryOut& og3,
          GeometryOut& og4, GeometryOut& og5, GeometryOut& og6)
    {
        typedef typename point_type<Geometry1>::type point1_type;

        // Get the envelop of inputs
        model::box<point1_type> env;
        geometry::assign_inverse(env);
        geometry::expand(env, g1);
        geometry::expand(env, g2);
        geometry::expand(env, g3);
        geometry::expand(env, g4);
        geometry::expand(env, g5);
        geometry::expand(env, g6);

        // Scale this to integer-range
        typename geometry::coordinate_type<point1_type>::type diff = get_max_size(env);
        double range = 1000000000.0; // Define a large range to get precise integer coordinates
        double factor = double(boost::long_long_type(range / double(diff)));

        // Assign input/output minimal points
        point1_type min_point1;
        detail::assign_point_from_index<0>(env, min_point1);

        typedef typename point_type<GeometryOut>::type point2_type;
        point2_type min_point2;
        assign_values(min_point2, boost::long_long_type(-range/2.0), boost::long_long_type(-range/2.0));

        detail::zoom_to_robust::rescale_strategy<point1_type, point2_type, double> strategy(min_point1, min_point2, factor);

        geometry::recalculate(og1, g1, strategy);
        geometry::recalculate(og2, g2, strategy);
        geometry::recalculate(og3, g3, strategy);
        geometry::recalculate(og4, g4, strategy);
        geometry::recalculate(og5, g5, strategy);
        geometry::recalculate(og6, g6, strategy);
    }
};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH

template <typename CoordinateType>
struct robust_type
{
    typedef typename dispatch::robust_type
        <
            CoordinateType,
            typename boost::is_floating_point<CoordinateType>::type
        >::type type;
};


template <typename Geometry1, typename Geometry2>
inline void zoom_to_robust(Geometry1 const& g1a, Geometry1 const& g1b, Geometry2& g2a, Geometry2& g2b)
{
    typedef typename point_type<Geometry1>::type point1_type;
    typedef typename point_type<Geometry2>::type point2_type;

    point1_type min_point1;
    point2_type min_point2;

    // Get the envelop of inputs
    model::box<point1_type> env;
    envelope(g1a, env);
    expand(env, g1b);

    // Scale this to integer-range
    typename coordinate_type<point1_type>::type diff = get_max_size(env);
    double range = 1000000000.0; // Define a large range to get precise integer coordinates
    double factor = range / diff;

    // Assign input/output minimal points
    detail::assign_point_from_index<0>(env, min_point1);
    assign_values(min_point2, boost::long_long_type(-range/2.0), boost::long_long_type(-range/2.0));

    detail::zoom_to_robust::rescale_strategy<point1_type, point2_type, double> strategy(min_point1, min_point2, factor);
    recalculate(g2a, g1a, strategy);
    recalculate(g2b, g1b, strategy);
}

template
<
    typename Geometry1, typename Geometry2, typename Geometry3,
    typename Geometry4, typename Geometry5, typename Geometry6,
    typename GeometryOut
>
void zoom_to_robust(Geometry1 const& g1, Geometry2 const& g2, Geometry3 const& g3,
          Geometry4 const& g4, Geometry5 const& g5, Geometry6 const& g6,
          GeometryOut& og1, GeometryOut& og2, GeometryOut& og3,
          GeometryOut& og4, GeometryOut& og5, GeometryOut& og6)
{
    // Make FP robust (so dispatch to true), so float and double
    // Other types as int, boost::rational, or ttmath are considered to be already robust
    dispatch::zoom_to_robust
        <
            typename boost::is_floating_point
                <
                    typename geometry::coordinate_type<Geometry1>::type
                >::type
        >::apply(g1, g2, g3, g4, g5, g6, og1, og2, og3, og4, og5, og6);
}

// UTILITY METHODS (might be moved)

template <typename Point, typename RobustPoint, typename Geometry, typename Factor>
static inline void init_rescale_policy(Geometry const& geometry,
        Point& min_point,
        RobustPoint& min_robust_point,
        Factor& factor)
{
    // Get bounding boxes
    model::box<Point> env = geometry::return_envelope<model::box<Point> >(geometry);

    // Scale this to integer-range
    typename geometry::coordinate_type<Point>::type diff = get_max_size(env);
    double range = 10000000.0; // Define a large range to get precise integer coordinates
    factor = double(boost::long_long_type(0.5 + range / double(diff)));
    //factor = range / diff;

    // Assign input/output minimal points
    detail::assign_point_from_index<0>(env, min_point);
    assign_values(min_robust_point, boost::long_long_type(-range/2.0), boost::long_long_type(-range/2.0));
}

template <typename Point, typename RobustPoint, typename Geometry1, typename Geometry2, typename Factor>
static inline void init_rescale_policy(Geometry1 const& geometry1,
        Geometry2 const& geometry2,
        Point& min_point,
        RobustPoint& min_robust_point,
        Factor& factor)
{
    // Get bounding boxes
    model::box<Point> env = geometry::return_envelope<model::box<Point> >(geometry1);
    model::box<Point> env2 = geometry::return_envelope<model::box<Point> >(geometry2);
    geometry::expand(env, env2);

    // Scale this to integer-range
    typename geometry::coordinate_type<Point>::type diff = get_max_size(env);
    double range = 10000000.0; // Define a large range to get precise integer coordinates
    factor = double(boost::long_long_type(0.5 + range / double(diff)));
    //factor = range / diff;

    // Assign input/output minimal points
    detail::assign_point_from_index<0>(env, min_point);
    assign_values(min_robust_point, boost::long_long_type(-range/2.0), boost::long_long_type(-range/2.0));
}

namespace detail { namespace rescale
{

template
<
    typename Point,
    bool IsFloatingPoint
>
struct rescale_policy_type
{
    typedef no_rescale_policy type;
};

// We rescale only all FP types
template
<
    typename Point
>
struct rescale_policy_type<Point, true>
{
private:
    typedef model::point
    <
        typename geometry::robust_type<typename geometry::coordinate_type<Point>::type>::type,
        geometry::dimension<Point>::value,
        typename geometry::coordinate_system<Point>::type
    > robust_point_type;
public:
    typedef detail::zoom_to_robust::rescale_strategy<Point, robust_point_type, double> type;
};

template <typename Policy>
struct get_rescale_policy
{
    template <typename Geometry>
    static inline Policy apply(Geometry const& geometry)
    {
        typedef typename point_type<Geometry>::type point_type;
        typedef typename geometry::coordinate_type<Geometry>::type coordinate_type;
        typedef model::point
        <
            typename geometry::robust_type<coordinate_type>::type,
            geometry::dimension<point_type>::value,
            typename geometry::coordinate_system<point_type>::type
        > robust_point_type;

        point_type min_point;
        robust_point_type min_robust_point;
        double factor;
        init_rescale_policy(geometry, min_point, min_robust_point, factor);

        return Policy(min_point, min_robust_point, factor);
    }

    template <typename Geometry1, typename Geometry2>
    static inline Policy apply(Geometry1 const& geometry1, Geometry2 const& geometry2)
    {
        typedef typename point_type<Geometry1>::type point_type;
        typedef typename geometry::coordinate_type<Geometry1>::type coordinate_type;
        typedef model::point
        <
            typename geometry::robust_type<coordinate_type>::type,
            geometry::dimension<point_type>::value,
            typename geometry::coordinate_system<point_type>::type
        > robust_point_type;

        point_type min_point;
        robust_point_type min_robust_point;
        double factor;
        init_rescale_policy(geometry1, geometry2, min_point, min_robust_point, factor);

        return Policy(min_point, min_robust_point, factor);
    }
};

// Specialization for no-rescaling
template <>
struct get_rescale_policy<no_rescale_policy>
{
    template <typename Geometry>
    static inline no_rescale_policy apply(Geometry const& )
    {
        no_rescale_policy result;
        return result;
    }

    template <typename Geometry1, typename Geometry2>
    static inline no_rescale_policy apply(Geometry1 const& , Geometry2 const& )
    {
        no_rescale_policy result;
        return result;
    }
};

}} // namespace detail::rescale

template<typename Point>
struct rescale_policy_type
    : public detail::rescale::rescale_policy_type
    <
        Point,
        boost::is_floating_point
        <
            typename geometry::coordinate_type<Point>::type
        >::type::value
    >
{
    BOOST_STATIC_ASSERT
    (
        boost::is_same
        <
            typename geometry::tag<Point>::type,
            geometry::point_tag
        >::type::value
    );
};

template <typename Policy, typename Geometry>
inline Policy get_rescale_policy(Geometry const& geometry)
{
    return detail::rescale::get_rescale_policy<Policy>::apply(geometry);
}

template <typename Policy, typename Geometry1, typename Geometry2>
inline Policy get_rescale_policy(Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    return detail::rescale::get_rescale_policy<Policy>::apply(geometry1, geometry2);
}


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_ZOOM_TO_ROBUST_HPP
