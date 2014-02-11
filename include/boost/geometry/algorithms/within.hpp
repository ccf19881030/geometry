// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2012 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2008-2012 Bruno Lalande, Paris, France.
// Copyright (c) 2009-2012 Mateusz Loskot, London, UK.

// This file was modified by Oracle on 2013.
// Modifications copyright (c) 2013, Oracle and/or its affiliates.

// Parts of Boost.Geometry are redesigned from Geodan's Geographic Library
// (geolib/GGL), copyright (c) 1995-2010 Geodan, Amsterdam, the Netherlands.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_WITHIN_HPP
#define BOOST_GEOMETRY_ALGORITHMS_WITHIN_HPP


#include <cstddef>

#include <boost/concept_check.hpp>
#include <boost/range.hpp>
#include <boost/typeof/typeof.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/variant_fwd.hpp>

#include <boost/geometry/algorithms/make.hpp>
#include <boost/geometry/algorithms/not_implemented.hpp>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/closure.hpp>
#include <boost/geometry/core/cs.hpp>
#include <boost/geometry/core/exterior_ring.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/point_order.hpp>
#include <boost/geometry/core/ring_type.hpp>
#include <boost/geometry/core/interior_rings.hpp>
#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/geometries/concepts/check.hpp>
#include <boost/geometry/policies/robustness/no_rescale_policy.hpp>
#include <boost/geometry/policies/robustness/segment_ratio_type.hpp>
#include <boost/geometry/strategies/concepts/within_concept.hpp>
#include <boost/geometry/strategies/default_strategy.hpp>
#include <boost/geometry/strategies/within.hpp>
#include <boost/geometry/util/math.hpp>
#include <boost/geometry/util/order_as_direction.hpp>
#include <boost/geometry/views/closeable_view.hpp>
#include <boost/geometry/views/reversible_view.hpp>

#include <boost/geometry/algorithms/detail/within/point_in_geometry.hpp>

#include <boost/geometry/algorithms/detail/overlay/get_turns.hpp>
#include <boost/geometry/algorithms/detail/overlay/do_reverse.hpp>
#include <deque>

namespace boost { namespace geometry
{

#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace within {

// currently works only for linestrings
template <typename Geometry1, typename Geometry2>
struct linear_linear
{
    template <typename Strategy>
    static inline bool apply(Geometry1 const& geometry1, Geometry2 const& geometry2, Strategy const& /*strategy*/)
    {
        // TODO within() should return FALSE if a length(geometry1) == 0 and is contained entirely within a boundary,
        // also if geometry2 has length(geometry2) == 0, it should be considered as a point.

        // TODO: currently only for linestrings
        std::size_t s1 = boost::size(geometry1);
        std::size_t s2 = boost::size(geometry2);
        if ( s1 == 0 || s2 == 0 || s2 == 1 )
            return false;
        if ( s1 == 1 && s2 == 1 ) // within(Pt, Pt)
            return false;
        if ( s1 == 1 )
            return point_in_geometry(*boost::begin(geometry1), geometry2) > 0;

        typedef detail::no_rescale_policy rescale_policy_type;
        typedef typename geometry::point_type<Geometry1>::type point1_type;
        typedef detail::overlay::turn_info
            <
                point1_type,
                typename segment_ratio_type<point1_type, rescale_policy_type>::type
            > turn_info;
        typedef detail::overlay::get_turn_info
            <
                detail::overlay::assign_null_policy
            > policy_type;

        std::deque<turn_info> turns;

        rescale_policy_type rescale_policy;
        detail::get_turns::no_interrupt_policy policy;
        boost::geometry::get_turns
                <
                    detail::overlay::do_reverse<geometry::point_order<Geometry1>::value>::value, // should be false
                    detail::overlay::do_reverse<geometry::point_order<Geometry2>::value>::value, // should be false
                    detail::overlay::assign_null_policy
                >(geometry1, geometry2, rescale_policy, turns, policy);

        return analyse_turns(turns.begin(), turns.end())
            // TODO: currently only for linestrings
            && point_in_geometry(*boost::begin(geometry1), geometry2) >= 0
            && point_in_geometry(*(boost::end(geometry1) - 1), geometry2) >= 0;
    }

    template <typename TurnIt>
    static inline bool analyse_turns(TurnIt first, TurnIt last)
    {
        bool has_turns = false;
        for ( TurnIt it = first ; it != last ; ++it )
        {
            switch ( it->method )
            {
                case overlay::method_crosses:
                    return false;
                case overlay::method_touch:
                case overlay::method_touch_interior:
                    if ( it->both(overlay::operation_continue)
                      || it->both(overlay::operation_blocked) )
                    {
                        has_turns = true;
                    }
                    else
                        return false;
                case overlay::method_equal:
                case overlay::method_collinear:
                    has_turns = true;
                    break;
                case overlay::method_none :
                case overlay::method_disjoint :
                case overlay::method_error :
                    break;
            }
        }

        return has_turns;
    }
};

}} // namespace detail::within
#endif // DOXYGEN_NO_DETAIL

#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{

template
<
    typename Geometry1,
    typename Geometry2,
    typename Tag1 = typename tag<Geometry1>::type,
    typename Tag2 = typename tag<Geometry2>::type
>
struct within: not_implemented<Tag1, Tag2>
{};


template <typename Point, typename Box>
struct within<Point, Box, point_tag, box_tag>
{
    template <typename Strategy>
    static inline bool apply(Point const& point, Box const& box, Strategy const& strategy)
    {
        boost::ignore_unused_variable_warning(strategy);
        return strategy.apply(point, box);
    }
};

template <typename Box1, typename Box2>
struct within<Box1, Box2, box_tag, box_tag>
{
    template <typename Strategy>
    static inline bool apply(Box1 const& box1, Box2 const& box2, Strategy const& strategy)
    {
        assert_dimension_equal<Box1, Box2>();
        boost::ignore_unused_variable_warning(strategy);
        return strategy.apply(box1, box2);
    }
};



template <typename Point, typename Ring>
struct within<Point, Ring, point_tag, ring_tag>
{
    template <typename Strategy>
    static inline bool apply(Point const& point, Ring const& ring, Strategy const& strategy)
    {
        return detail::within::point_in_geometry(point, ring, strategy) == 1;
    }
};

template <typename Point, typename Polygon>
struct within<Point, Polygon, point_tag, polygon_tag>
{
    template <typename Strategy>
    static inline bool apply(Point const& point, Polygon const& polygon, Strategy const& strategy)
    {
        return detail::within::point_in_geometry(point, polygon, strategy) == 1;
    }
};

template <typename Point, typename Linestring>
struct within<Point, Linestring, point_tag, linestring_tag>
{
    template <typename Strategy> static inline
    bool apply(Point const& point, Linestring const& linestring, Strategy const& strategy)
    {
        return detail::within::point_in_geometry(point, linestring, strategy) == 1;
    }
};

template <typename Linestring1, typename Linestring2>
struct within<Linestring1, Linestring2, linestring_tag, linestring_tag>
{
    template <typename Strategy> static inline
    bool apply(Linestring1 const& linestring1, Linestring2 const& linestring2, Strategy const& strategy)
    {
        return detail::within::linear_linear<Linestring1, Linestring2>
                ::apply(linestring1, linestring2, strategy);
    }
};

} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH


namespace resolve_strategy
{

struct within
{
    template <typename Geometry1, typename Geometry2, typename Strategy>
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             Strategy const& strategy)
    {
        concept::within::check
            <
                typename tag<Geometry1>::type,
                typename tag<Geometry2>::type,
                typename tag_cast<typename tag<Geometry2>::type, areal_tag>::type,
                Strategy
            >();

        return dispatch::within<Geometry1, Geometry2>::apply(geometry1, geometry2, strategy);
    }

    template <typename Geometry1, typename Geometry2>
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             default_strategy)
    {
        typedef typename point_type<Geometry1>::type point_type1;
        typedef typename point_type<Geometry2>::type point_type2;

        typedef typename strategy::within::services::default_strategy
            <
                typename tag<Geometry1>::type,
                typename tag<Geometry2>::type,
                typename tag<Geometry1>::type,
                typename tag_cast<typename tag<Geometry2>::type, areal_tag>::type,
                typename tag_cast
                    <
                        typename cs_tag<point_type1>::type, spherical_tag
                    >::type,
                typename tag_cast
                    <
                        typename cs_tag<point_type2>::type, spherical_tag
                    >::type,
                Geometry1,
                Geometry2
            >::type strategy_type;

        return apply(geometry1, geometry2, strategy_type());
    }
};

} // namespace resolve_strategy


namespace resolve_variant
{

template <typename Geometry1, typename Geometry2>
struct within
{
    template <typename Strategy>
    static inline bool apply(Geometry1 const& geometry1,
                             Geometry2 const& geometry2,
                             Strategy const& strategy)
    {
        concept::check<Geometry1 const>();
        concept::check<Geometry2 const>();
        assert_dimension_equal<Geometry1, Geometry2>();

        return resolve_strategy::within::apply(geometry1,
                                               geometry2,
                                               strategy);
    }
};

template <BOOST_VARIANT_ENUM_PARAMS(typename T), typename Geometry2>
struct within<boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)>, Geometry2>
{
    template <typename Strategy>
    struct visitor: boost::static_visitor<bool>
    {
        Geometry2 const& m_geometry2;
        Strategy const& m_strategy;

        visitor(Geometry2 const& geometry2, Strategy const& strategy):
        m_geometry2(geometry2),
        m_strategy(strategy)
        {}

        template <typename Geometry1>
        bool operator()(Geometry1 const& geometry1) const
        {
            return within<Geometry1, Geometry2>::apply(geometry1,
                                                       m_geometry2,
                                                       m_strategy);
        }
    };

    template <typename Strategy>
    static inline bool
    apply(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& geometry1,
          Geometry2 const& geometry2,
          Strategy const& strategy)
    {
        return boost::apply_visitor(
            visitor<Strategy>(geometry2, strategy),
            geometry1
        );
    }
};

template <typename Geometry1, BOOST_VARIANT_ENUM_PARAMS(typename T)>
struct within<Geometry1, boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> >
{
    template <typename Strategy>
    struct visitor: boost::static_visitor<bool>
    {
        Geometry1 const& m_geometry1;
        Strategy const& m_strategy;

        visitor(Geometry1 const& geometry1, Strategy const& strategy):
        m_geometry1(geometry1),
        m_strategy(strategy)
        {}

        template <typename Geometry2>
        bool operator()(Geometry2 const& geometry2) const
        {
            return within<Geometry1, Geometry2>::apply(m_geometry1,
                                                       geometry2,
                                                       m_strategy);
        }
    };

    template <typename Strategy>
    static inline bool
    apply(Geometry1 const& geometry1,
          boost::variant<BOOST_VARIANT_ENUM_PARAMS(T)> const& geometry2,
          Strategy const& strategy)
    {
        return boost::apply_visitor(
            visitor<Strategy>(geometry1, strategy),
            geometry2
        );
    }
};

template <
    BOOST_VARIANT_ENUM_PARAMS(typename T1),
    BOOST_VARIANT_ENUM_PARAMS(typename T2)
>
struct within<
    boost::variant<BOOST_VARIANT_ENUM_PARAMS(T1)>,
    boost::variant<BOOST_VARIANT_ENUM_PARAMS(T2)>
>
{
    template <typename Strategy>
    struct visitor: boost::static_visitor<bool>
    {
        Strategy const& m_strategy;

        visitor(Strategy const& strategy): m_strategy(strategy) {}

        template <typename Geometry1, typename Geometry2>
        bool operator()(Geometry1 const& geometry1,
                        Geometry2 const& geometry2) const
        {
            return within<Geometry1, Geometry2>::apply(geometry1,
                                                       geometry2,
                                                       m_strategy);
        }
    };

    template <typename Strategy>
    static inline bool
    apply(boost::variant<BOOST_VARIANT_ENUM_PARAMS(T1)> const& geometry1,
          boost::variant<BOOST_VARIANT_ENUM_PARAMS(T2)> const& geometry2,
          Strategy const& strategy)
    {
        return boost::apply_visitor(
            visitor<Strategy>(strategy),
            geometry1, geometry2
        );
    }
};

}


/*!
\brief \brief_check12{is completely inside}
\ingroup within
\details \details_check12{within, is completely inside}.
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry which might be within the second geometry
\param geometry2 \param_geometry which might contain the first geometry
\return true if geometry1 is completely contained within geometry2,
    else false
\note The default strategy is used for within detection


\qbk{[include reference/algorithms/within.qbk]}

\qbk{
[heading Example]
[within]
[within_output]
}
 */
template<typename Geometry1, typename Geometry2>
inline bool within(Geometry1 const& geometry1, Geometry2 const& geometry2)
{
    return resolve_variant::within
        <
            Geometry1,
            Geometry2
        >::apply(geometry1, geometry2, default_strategy());
}

/*!
\brief \brief_check12{is completely inside} \brief_strategy
\ingroup within
\details \details_check12{within, is completely inside}, \brief_strategy. \details_strategy_reasons
\tparam Geometry1 \tparam_geometry
\tparam Geometry2 \tparam_geometry
\param geometry1 \param_geometry which might be within the second geometry
\param geometry2 \param_geometry which might contain the first geometry
\param strategy strategy to be used
\return true if geometry1 is completely contained within geometry2,
    else false

\qbk{distinguish,with strategy}
\qbk{[include reference/algorithms/within.qbk]}
\qbk{
[heading Available Strategies]
\* [link geometry.reference.strategies.strategy_within_winding Winding (coordinate system agnostic)]
\* [link geometry.reference.strategies.strategy_within_franklin Franklin (cartesian)]
\* [link geometry.reference.strategies.strategy_within_crossings_multiply Crossings Multiply (cartesian)]

[heading Example]
[within_strategy]
[within_strategy_output]

}
*/
template<typename Geometry1, typename Geometry2, typename Strategy>
inline bool within(Geometry1 const& geometry1, Geometry2 const& geometry2,
        Strategy const& strategy)
{
    return resolve_variant::within
        <
            Geometry1,
            Geometry2
        >::apply(geometry1, geometry2, strategy);
}

}} // namespace boost::geometry

#endif // BOOST_GEOMETRY_ALGORITHMS_WITHIN_HPP
