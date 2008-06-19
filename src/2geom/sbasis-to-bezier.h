#ifndef _SBASIS_TO_BEZIER
#define _SBASIS_TO_BEZIER

#include "d2.h"
#include "path.h"

namespace Geom{
// this produces a degree k bezier from a degree k sbasis
Bezier
sbasis_to_bezier(SBasis const &B, unsigned q = 0);

// inverse
SBasis bezier_to_sbasis(Bezier const &B);


std::vector<Geom::Point>
sbasis_to_bezier(D2<SBasis> const &B, unsigned q = 0);

std::vector<Path> path_from_piecewise(Piecewise<D2<SBasis> > const &B, double tol);

Path path_from_sbasis(D2<SBasis> const &B, double tol);

};
#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :