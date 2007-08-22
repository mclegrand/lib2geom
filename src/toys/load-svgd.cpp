#include "d2.h"
#include "sbasis.h"

#include "shape.h"
#include "path.h"
#include "svg-path-parser.h"

#include "path-cairo.h"
#include "toy-framework.cpp"
#include "transforms.h"
#include "sbasis-geometric.h"

#include <cstdlib>

using namespace Geom;

void cairo_region(cairo_t *cr, Region const &r) {
    cairo_set_source_rgba(cr, 0, 0, 0, 1); //rand_d(), rand_d(), rand_d(), .75);
    double d = 5.;
    if(!r.isFill()) cairo_set_dash(cr, &d, 1, 0);
    cairo_path(cr, r.getBoundary());
    cairo_stroke(cr);
    cairo_set_dash(cr, &d, 0, 0);
}

void cairo_regions(cairo_t *cr, Regions const &p) {
    srand(0); 
    for(Regions::const_iterator j = p.begin(); j != p.end(); j++)
        cairo_region(cr, *j);
}

void cairo_shape(cairo_t *cr, Shape const &s) {
    cairo_regions(cr, s.getContent());
}

Shape cleanup(std::vector<Path> const &ps) {
    Regions rs = regions_from_paths(ps);
    
    for(unsigned i = 0; i < rs.size(); i++) {
        Point exemplar = rs[i].getBoundary().initialPoint();
        for(unsigned j = 0; j < rs.size(); j++) {
            if(i != j && rs[j].contains(exemplar)) {
                if(rs[i].isFill()) rs[i] = rs[i].inverse();
                if(!rs[j].isFill()) rs[j] = rs[j].inverse();
                goto next;
            }
        }
        if(!rs[i].isFill()) rs[i] = rs[i].inverse();
        next: (void)0;
    }
    
    Piecewise<D2<SBasis> > pw = paths_to_pw(ps);
    double area;
    Point centre;
    Geom::centroid(pw, centre, area);
    
    return Shape(rs) * Geom::Translate(-centre);
}

class BoolOps: public Toy {
    Region b;
    Shape bs;
    virtual void draw(cairo_t *cr, std::ostringstream *notify, int width, int height, bool save) {
        Geom::Translate t(handles[0]);
        Shape bst = bs * t;
        Region bt = Region(b.getBoundary() * t, b.isFill());
        
        cairo_set_line_width(cr, 1);
        
        cairo_shape(cr, bst);

        Toy::draw(cr, notify, width, height, save);
    }
    public:
    BoolOps () {}

    void first_time(int argc, char** argv) {
        char *path_b_name="star.svgd";
        if(argc > 1)
            path_b_name = argv[1];
        std::vector<Path> paths_b = read_svgd(path_b_name);
        
	Rect bounds = paths_b[0].boundsExact();
	
        handles.push_back(bounds.midpoint());

        bs = cleanup(paths_b);
        b = bs.getContent().front();
    }
    int should_draw_bounds() {return 0;}
};

int main(int argc, char **argv) {
    init(argc, argv, new BoolOps());
    return 0;
}

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=4:softtabstop=4:encoding=utf-8:textwidth=99 :
