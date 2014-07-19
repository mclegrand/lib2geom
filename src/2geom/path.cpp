/** @file
 * @brief Path - a sequence of contiguous curves (implementation file)
 *//*
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Marco Cecchetti <mrcekets at gmail.com>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright 2007-2014  authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 */

#include <2geom/path.h>
#include <2geom/pathvector.h>
#include <2geom/transforms.h>
#include <algorithm>
#include <limits>

using std::swap;
using namespace Geom::PathInternal;

namespace Geom {

void Path::clear() {
    _unshare();
    _curves->pop_back().release();
    _curves->clear();
    _closing_seg->setInitial(Point(0, 0));
    _closing_seg->setFinal(Point(0, 0));
    _curves->push_back(_closing_seg);
    _closed = false;
}

OptRect Path::boundsFast() const
{
    OptRect bounds;
    if (empty())
        return bounds;
    bounds = front().boundsFast();
    const_iterator iter = begin();
    // the closing path segment can be ignored, because it will always lie within the bbox of the rest of the path
    if (iter != end()) {
        for (++iter; iter != end(); ++iter) {
            bounds.unionWith(iter->boundsFast());
        }
    }
    return bounds;
}

OptRect Path::boundsExact() const
{
    OptRect bounds;
    if (empty())
        return bounds;
    bounds = front().boundsExact();
    const_iterator iter = begin();
    // the closing path segment can be ignored, because it will always lie within the bbox of the rest of the path
    if (iter != end()) {
        for (++iter; iter != end(); ++iter) {
            bounds.unionWith(iter->boundsExact());
        }
    }
    return bounds;
}

Piecewise<D2<SBasis> > Path::toPwSb() const
{
    Piecewise<D2<SBasis> > ret;
    ret.push_cut(0);
    unsigned i = 1;
    bool degenerate = true;
    // pw<d2<>> is always open. so if path is closed, add closing segment as well to pwd2.
    for (const_iterator it = begin(); it != end_default(); ++it) {
        if (!it->isDegenerate()) {
            ret.push(it->toSBasis(), i++);
            degenerate = false;
        }
    }
    if (degenerate) {
        // if path only contains degenerate curves, no second cut is added
        // so we need to create at least one segment manually
        ret = Piecewise<D2<SBasis> >(initialPoint());
    }
    return ret;
}

template <typename iter>
iter inc(iter const &x, unsigned n) {
    iter ret = x;
    for (unsigned i = 0; i < n; i++)
        ret++;
    return ret;
}

bool Path::operator==(Path const &other) const
{
    if (this == &other)
        return true;
    if (_closed != other._closed)
        return false;
    return *_curves == *other._curves;
}

Path &Path::operator*=(Affine const &m)
{
    _unshare();
    Sequence::iterator last = _curves->end() - 1;
    Sequence::iterator it;
    Point prev;
    for (it = _curves->begin(); it != last; ++it) {
        _curves->replace(it, it->transformed(m));
        if (it != _curves->begin() && it->initialPoint() != prev) {
            THROW_CONTINUITYERROR();
        }
        prev = it->finalPoint();
    }
    for (int i = 0; i < 2; ++i) {
        _closing_seg->setPoint(i, (*_closing_seg)[i] * m);
    }
    if (_curves->size() > 1) {
        if (front().initialPoint() != initialPoint() || back().finalPoint() != finalPoint()) {
            THROW_CONTINUITYERROR();
        }
    }
    return *this;
}

Path &Path::operator*=(Translate const &m)
{
    /* Somehow there is something wrong here, Inkscape's LPE Construct grid fails with this code, perhaps something with
    desharing of curves...
    _unshare();
    Sequence::iterator last = _curves->end() - 1;
    Sequence::iterator it;
    Point prev;
    for (it = _curves->begin() ; it != last ; ++it) {
      *(const_cast<Curve*>(&**it)) *= m;
      if ( it != _curves->begin() && (*it)->initialPoint() != prev ) {
        THROW_CONTINUITYERROR();
      }
      prev = (*it)->finalPoint();
    }
    for ( int i = 0 ; i < 2 ; ++i ) {
      _closing_seg->setPoint(i, (*_closing_seg)[i] + m.vector());
    }
    if (_curves->size() > 1) {
      if ( front().initialPoint() != initialPoint() || back().finalPoint() != finalPoint() ) {
        THROW_CONTINUITYERROR();
      }
    }
    return *this;
    */
    return this->operator*=(static_cast<Affine>(m));
}

void Path::start(Point const &p) {
    if (_curves->size() > 1) {
        clear();
    }
    _closing_seg->setInitial(p);
    _closing_seg->setFinal(p);
}

Interval Path::timeRange() const
{
    Interval ret(0, size_default());
    return ret;
}

Curve const &Path::curveAt(Coord t, Coord *rest) const
{
    Position pos = _getPosition(t);
    if (rest) {
        *rest = pos.t;
    }
    return at(pos.curve_index);
}

Point Path::pointAt(Coord t) const
{
    return pointAt(_getPosition(t));
}

Coord Path::valueAt(Coord t, Dim2 d) const
{
    return valueAt(_getPosition(t), d);
}

Curve const &Path::curveAt(Position const &pos) const
{
    return at(pos.curve_index);
}
Point Path::pointAt(Position const &pos) const
{
    return at(pos.curve_index).pointAt(pos.t);
}
Coord Path::valueAt(Position const &pos, Dim2 d) const
{
    return at(pos.curve_index).valueAt(pos.t, d);
}

std::vector<Coord> Path::roots(Coord v, Dim2 d) const
{
    std::vector<Coord> res;
    for (unsigned i = 0; i <= size(); i++) {
        std::vector<Coord> temp = (*this)[i].roots(v, d);
        for (unsigned j = 0; j < temp.size(); j++)
            res.push_back(temp[j] + i);
    }
    return res;
}

std::vector<double> Path::allNearestTimes(Point const &_point, double from, double to) const
{
    // TODO from and to are not used anywhere.
    // rewrite this to simplify.
    using std::swap;

    if (from > to)
        swap(from, to);
    const Path &_path = *this;
    unsigned int sz = _path.size();
    if (_path.closed())
        ++sz;
    if (from < 0 || to > sz) {
        THROW_RANGEERROR("[from,to] interval out of bounds");
    }
    double sif, st = modf(from, &sif);
    double eif, et = modf(to, &eif);
    unsigned int si = static_cast<unsigned int>(sif);
    unsigned int ei = static_cast<unsigned int>(eif);
    if (si == sz) {
        --si;
        st = 1;
    }
    if (ei == sz) {
        --ei;
        et = 1;
    }
    if (si == ei) {
        std::vector<double> all_nearest = _path[si].allNearestTimes(_point, st, et);
        for (unsigned int i = 0; i < all_nearest.size(); ++i) {
            all_nearest[i] = si + all_nearest[i];
        }
        return all_nearest;
    }
    std::vector<double> all_t;
    std::vector<std::vector<double> > all_np;
    all_np.push_back(_path[si].allNearestTimes(_point, st));
    std::vector<unsigned int> ni;
    ni.push_back(si);
    double dsq;
    double mindistsq = distanceSq(_point, _path[si].pointAt(all_np.front().front()));
    Rect bb(Geom::Point(0, 0), Geom::Point(0, 0));
    for (unsigned int i = si + 1; i < ei; ++i) {
        bb = (_path[i].boundsFast());
        dsq = distanceSq(_point, bb);
        if (mindistsq < dsq)
            continue;
        all_t = _path[i].allNearestTimes(_point);
        dsq = distanceSq(_point, _path[i].pointAt(all_t.front()));
        if (mindistsq > dsq) {
            all_np.clear();
            all_np.push_back(all_t);
            ni.clear();
            ni.push_back(i);
            mindistsq = dsq;
        } else if (mindistsq == dsq) {
            all_np.push_back(all_t);
            ni.push_back(i);
        }
    }
    bb = (_path[ei].boundsFast());
    dsq = distanceSq(_point, bb);
    if (mindistsq >= dsq) {
        all_t = _path[ei].allNearestTimes(_point, 0, et);
        dsq = distanceSq(_point, _path[ei].pointAt(all_t.front()));
        if (mindistsq > dsq) {
            for (unsigned int i = 0; i < all_t.size(); ++i) {
                all_t[i] = ei + all_t[i];
            }
            return all_t;
        } else if (mindistsq == dsq) {
            all_np.push_back(all_t);
            ni.push_back(ei);
        }
    }
    std::vector<double> all_nearest;
    for (unsigned int i = 0; i < all_np.size(); ++i) {
        for (unsigned int j = 0; j < all_np[i].size(); ++j) {
            all_nearest.push_back(ni[i] + all_np[i][j]);
        }
    }
    all_nearest.erase(std::unique(all_nearest.begin(), all_nearest.end()), all_nearest.end());
    return all_nearest;
}

std::vector<Coord> Path::nearestTimePerCurve(Point const &p) const
{
    // return a single nearest time for each curve in this path
    std::vector<Coord> np;
    for (const_iterator it = begin(); it != end_default(); ++it) {
        np.push_back(it->nearestTime(p));
    }
    return np;
}

Coord Path::nearestTime(Point const &p, Coord *dist) const
{
    Position pos = nearestPosition(p, dist);
    return pos.curve_index + pos.t;
}

PathPosition Path::nearestPosition(Point const &p, Coord *dist) const
{
    Coord mindist = std::numeric_limits<Coord>::max();
    Position ret;

    if (_curves->size() == 1) {
        // naked moveto
        ret.curve_index = 0;
        ret.t = 0;
        if (dist) {
            *dist = distance(_closing_seg->initialPoint(), p);
        }
        return ret;
    }

    for (size_type i = 0; i < size_default(); ++i) {
        Curve const &c = at(i);
        if (distance(p, c.boundsFast()) >= mindist) continue;

        Coord t = c.nearestTime(p);
        Coord d = distance(c.pointAt(t), p);
        if (d < mindist) {
            mindist = d;
            ret.curve_index = i;
            ret.t = t;
        }
    }
    if (dist) {
        *dist = mindist;
    }

    return ret;
}

void Path::appendPortionTo(Path &ret, double from, double to) const
{
    if (!(from >= 0 && to >= 0)) {
        THROW_RANGEERROR("from and to must be >=0 in Path::appendPortionTo");
    }
    if (to == 0)
        to = size() + 0.999999;
    if (from == to) {
        return;
    }
    double fi, ti;
    double ff = modf(from, &fi), tf = modf(to, &ti);
    if (tf == 0) {
        ti--;
        tf = 1;
    }
    const_iterator fromi = inc(begin(), (unsigned)fi);
    if (fi == ti && from < to) {
        Curve *v = fromi->portion(ff, tf);
        ret.append(*v, STITCH_DISCONTINUOUS);
        delete v;
        return;
    }
    const_iterator toi = inc(begin(), (unsigned)ti);
    if (ff != 1.) {
        Curve *fromv = fromi->portion(ff, 1.);
        // fromv->setInitial(ret.finalPoint());
        ret.append(*fromv, STITCH_DISCONTINUOUS);
        delete fromv;
    }
    if (from >= to) {
        const_iterator ender = end();
        if (ender->initialPoint() == ender->finalPoint())
            ++ender;
        ret.insert(ret.end(), ++fromi, ender, STITCH_DISCONTINUOUS);
        ret.insert(ret.end(), begin(), toi, STITCH_DISCONTINUOUS);
    } else {
        ret.insert(ret.end(), ++fromi, toi, STITCH_DISCONTINUOUS);
    }
    Curve *tov = toi->portion(0., tf);
    ret.append(*tov, STITCH_DISCONTINUOUS);
    delete tov;
}

Path Path::reversed() const
{
    typedef std::reverse_iterator<Sequence::const_iterator> RIter;

    Path ret;
    ret._curves->pop_back();
    RIter iter(_curves->end()), rend(_curves->begin());
    for (; iter != rend; ++iter) {
        ret._curves->push_back(iter->reverse());
    }
    ret._closing_seg = static_cast<ClosingSegment *>(ret._closing_seg->reverse());
    ret._curves->push_back(ret._closing_seg);
    return ret;
}


void Path::insert(iterator pos, Curve const &curve, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_pos(seq_iter(pos));
    Sequence source;
    source.push_back(curve.duplicate());
    if (stitching)
        stitch(seq_pos, seq_pos, source);
    do_update(seq_pos, seq_pos, source.begin(), source.end(), source);
}

void Path::insert(iterator pos, const_iterator first, const_iterator last, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_pos(seq_iter(pos));
    Sequence source(seq_iter(first), seq_iter(last));
    if (stitching)
        stitch(seq_pos, seq_pos, source);
    do_update(seq_pos, seq_pos, source.begin(), source.end(), source);
}

void Path::erase(iterator pos, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_pos(seq_iter(pos));
    if (stitching) {
        Sequence stitched;
        stitch(seq_pos, seq_pos + 1, stitched);
        do_update(seq_pos, seq_pos + 1, stitched.begin(), stitched.end(), stitched);
    } else {
        do_update(seq_pos, seq_pos + 1, _curves->begin(), _curves->begin(), *_curves);
    }
}

void Path::erase(iterator first, iterator last, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_first = seq_iter(first);
    Sequence::iterator seq_last = seq_iter(last);
    if (stitching) {
        Sequence stitched;
        stitch(seq_first, seq_last, stitched);
        do_update(seq_first, seq_last, stitched.begin(), stitched.end(), stitched);
    } else {
        do_update(seq_first, seq_last, _curves->begin(), _curves->begin(), *_curves);
    }
}

void Path::replace(iterator replaced, Curve const &curve, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_replaced(seq_iter(replaced));
    Sequence source(1);
    source.push_back(curve.duplicate());
    if (stitching)
        stitch(seq_replaced, seq_replaced + 1, source);
    do_update(seq_replaced, seq_replaced + 1, source.begin(), source.end(), source);
}

void Path::replace(iterator first_replaced, iterator last_replaced, Curve const &curve,
                   Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_first_replaced(seq_iter(first_replaced));
    Sequence::iterator seq_last_replaced(seq_iter(last_replaced));
    Sequence source(1);
    source.push_back(curve.duplicate());
    if (stitching)
        stitch(seq_first_replaced, seq_last_replaced, source);
    do_update(seq_first_replaced, seq_last_replaced, source.begin(), source.end(), source);
}

void Path::replace(iterator replaced, const_iterator first, const_iterator last,
                   Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_replaced(seq_iter(replaced));
    Sequence source(seq_iter(first), seq_iter(last));
    if (stitching)
        stitch(seq_replaced, seq_replaced + 1, source);
    do_update(seq_replaced, seq_replaced + 1, source.begin(), source.end(), source);
}

void Path::replace(iterator first_replaced, iterator last_replaced, const_iterator first,
                   const_iterator last, Stitching stitching)
{
    _unshare();
    Sequence::iterator seq_first_replaced(seq_iter(first_replaced));
    Sequence::iterator seq_last_replaced(seq_iter(last_replaced));
    Sequence source(seq_iter(first), seq_iter(last));
    if (stitching)
        stitch(seq_first_replaced, seq_last_replaced, source);
    do_update(seq_first_replaced, seq_last_replaced, source.begin(), source.end(), source);
}

void Path::do_update(Sequence::iterator first_replaced, Sequence::iterator last_replaced, Sequence::iterator first,
                     Sequence::iterator last, Sequence &source)
{
    _curves->erase(first_replaced, last_replaced);
    _curves->transfer(first_replaced, first, last, source);

    if (&_curves->front() != _closing_seg) {
        _closing_seg->setPoint(0, back().finalPoint());
        _closing_seg->setPoint(1, front().initialPoint());
    }

    checkContinuity();
}

void Path::do_append(Curve *c)
{
    if (&_curves->front() == _closing_seg) {
        _closing_seg->setFinal(c->initialPoint());
    } else {
        if (c->initialPoint() != finalPoint()) {
            THROW_CONTINUITYERROR();
        }
    }
    _curves->insert(_curves->end() - 1, c);
    _closing_seg->setInitial(c->finalPoint());
}

void Path::stitch(Sequence::iterator first_replaced, Sequence::iterator last_replaced, Sequence &source)
{
    if (!source.empty()) {
        if (first_replaced != _curves->begin()) {
            if (first_replaced->initialPoint() != source.front().initialPoint()) {
                Curve *stitch = new StitchSegment(first_replaced->initialPoint(), source.front().initialPoint());
                source.insert(source.begin(), stitch);
            }
        }
        if (last_replaced != (_curves->end() - 1)) {
            if (last_replaced->finalPoint() != source.back().finalPoint()) {
                Curve *stitch = new StitchSegment(source.back().finalPoint(), last_replaced->finalPoint());
                source.insert(source.end(), stitch);
            }
        }
    } else if (first_replaced != last_replaced && first_replaced != _curves->begin() &&
               last_replaced != _curves->end() - 1) {
        if (first_replaced->initialPoint() != (last_replaced - 1)->finalPoint()) {
            Curve *stitch = new StitchSegment((last_replaced - 1)->finalPoint(), first_replaced->initialPoint());
            source.insert(source.begin(), stitch);
        }
    }
}

void Path::checkContinuity() const
{
    Sequence::const_iterator i = _curves->begin(), j = _curves->begin();
    ++j;
    for (; j != _curves->end(); ++i, ++j) {
        if (i->finalPoint() != j->initialPoint()) {
            THROW_CONTINUITYERROR();
        }
    }
    if (_curves->front().initialPoint() != _curves->back().finalPoint()) {
        THROW_CONTINUITYERROR();
    }
}

PathPosition Path::_getPosition(Coord t) const
{
    size_type sz = size_default();
    if (t < 0 || t > sz) {
        THROW_RANGEERROR("parameter t out of bounds");
    }

    Position ret;
    Coord k;
    ret.t = modf(t, &k);
    ret.curve_index = k;
    if (ret.curve_index == sz) {
        --ret.curve_index;
        ret.t = 1;
    }
    return ret;
}

Piecewise<D2<SBasis> > paths_to_pw(PathVector const &paths)
{
    Piecewise<D2<SBasis> > ret = paths[0].toPwSb();
    for (unsigned i = 1; i < paths.size(); i++) {
        ret.concat(paths[i].toPwSb());
    }
    return ret;
}

} // end namespace Geom

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
