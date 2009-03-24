#include <iostream>
#include <2geom/path.h>
#include <2geom/svg-path-parser.h>
#include <2geom/path-intersection.h>
#include <2geom/basic-intersection.h>
#include <2geom/pathvector.h>

#include <cstdlib>

#include <2geom/toys/path-cairo.h>
#include <2geom/toys/toy-framework-2.h>

#include <algorithm>
#include <deque>

using namespace Geom;
using namespace std;

struct CurveIx {
    unsigned path, ix;
    CurveIx(unsigned p, unsigned i) : path(p), ix(i) {}
    Curve const &get(std::vector<Path> const &ps) const {
        return ps[path][ix];
    }
};

/*
struct WEvent {
    CurveIx curve;
    double time;
    Point point;
    unsigned vert;
    WEvent(CurveIx c, double t, Point p, unsigned v) : curve(c), time(t), point(p), vert(v) {}
}; */

struct Section {
    CurveIx curve;
    double f, t;
    Point fp, tp;
    unsigned fv, tv;
    Section(Curve const &c, Dim2 d, CurveIx cix, double fd, double td, unsigned fve, unsigned tve) : curve(cix), f(fd), t(td), fv(fve), tv(tve) {
        fp = c.pointAt(f), tp = c.pointAt(t);
        //TODO: decide which direction the second case should work
        if(fp[d] > tp[d] || (fp[d] == tp[d] && fp[1-d] < tp[1-d])) {
            //swap from and to
            std::swap(f, t);
            std::swap(fp, tp);
            std::swap(fv, tv);
        }
    }
    void set_to(Curve const &c, Dim2 d, double ti, unsigned v) {
        //we're assuming the to-val isn't before our from
        t = ti;
        tp = c(ti);
        tv = v;
        assert(tp[d] >= fp[d]);
    }
    Rect bbox() const {
        return Rect(fp, tp);
    }
};

std::vector<Rect> section_rects(std::vector<Section> const &s) {
    std::vector<Rect> ret;
    for(unsigned i = 0; i < s.size(); i++) {
        ret.push_back(s[i].bbox());
    }
    return ret;
}

void draw_section(cairo_t *cr, Section const &s, std::vector<Path> const &ps) {
    Interval ti = Interval(s.f, s.t);
    Curve *curv = s.curve.get(ps).portion(ti.min(), ti.max());
    cairo_curve(cr, *curv);
    delete curv;
}

// A little sugar for appending a list to another
template<typename T>
void append(T &a, T const &b) {
    a.insert(a.end(), b.begin(), b.end());
}

//yield a sorted, unique list of monotonic cuts of a curve, including 0 and 1
std::vector<double> mono_splits(Curve const &d) {
    Curve* deriv = d.derivative();
    std::vector<double> splits = deriv->roots(0, X);
    append(splits, deriv->roots(0, Y));
    delete deriv;
    splits.push_back(0);
    splits.push_back(1);
    std::sort(splits.begin(), splits.end());
    std::unique(splits.begin(), splits.end());
    return splits;
}

struct SectionSorter {
    const std::vector<Path> *ps;
    Dim2 dim;
    SectionSorter(const std::vector<Path> *rs, Dim2 d) : ps(rs), dim(d) {}
    bool operator()(Section x, Section y) {
        if(x.fp[dim] < y.fp[dim]) return true;
        if(x.fp[dim] > y.fp[dim]) return false;
        if(x.fp[1-dim] < y.fp[1-dim]) return true;
        if(x.fp[1-dim] > y.fp[1-dim]) return false;
        //TODO: path-based comparison
        
        return x.fv < y.fv;
    }
};

std::vector<std::vector<Section> > sweep_window(cairo_t *cr, std::vector<Path> const &ps) {
    std::vector<std::vector<Section> > contexts;
    std::vector<Section> context;
    
    unsigned vert = 0;
    //TODO: also store a priority queue of sectioned off bits, and do mergepass
    std::deque<Section> monos;
    
    for(unsigned i = 0; i < ps.size(); i++) {
        //TODO: necessary? can we have empty paths?
        if(ps[i].size()) {
            unsigned first = vert;
            for(unsigned j = 0; j < ps[i].size(); j++) {
                std::vector<double> splits = mono_splits(ps[i][j]);
                for(unsigned k = 1; k < splits.size(); k++) {
                   monos.push_back(Section(ps[i][j], X, CurveIx(i,j), splits[k-1], splits[k], vert, vert+1));
                   vert++;
                }
            }
            if(monos.back().fv == vert) monos.back().fv = first; else monos.back().tv = first;
        }
    }
    std::sort(monos.begin(), monos.end(), SectionSorter(&ps, X));

    while(!monos.empty()) {
        Section s = monos.front();
        monos.pop_front();

        int seg_ix = -1;
        for(unsigned i = 0; i < context.size(); i++) {
            if(context[i].tv == s.fv) {
                seg_ix = i;
                //we could probably break out at this point
            } else if(context[i].tp[X] < s.fp[X]) {
                context.erase(context.begin() + i); // remove irrelevant sections
                i--; //Must decrement, due to the removal
            }
        }
        if(seg_ix == -1) {
            //didn't find a preceding section
            //insert into context, in the proper location
            seg_ix = std::lower_bound(context.begin(), context.end(), s, SectionSorter(&ps, X)) - context.begin();
            context.insert(context.begin() + seg_ix, s);
        } else {
            // we found a preceding section! replace it with the new one
            context[seg_ix] = s;
        }
        //cout << s.curve.path << " " << s.curve.ix << " " << seg_ix << endl;
        
        //TODO: deal with multiple paths intersecting at a point
        
        // Now we intersect with neighbors - do a sweep!
        // we'll have to make the same decision as with construction - derivative checking
        // for each intersection point, create a fresh vertex, and assign the ends of sections to it
        std::vector<Rect> sing;
        sing.push_back(s.bbox());
        std::vector<unsigned> others = sweep_bounds(sing, section_rects(context), Y)[0];
        for(unsigned i = 0; i < others.size(); i++) {
            if(others[i] == seg_ix) continue;
            
            Section other = context[others[i]];
            //TODO: what we really need is a function that gets the first intersection
            //if not that, we need to chunk curves into multiple pieces properly
            Crossings xs = mono_intersect(s.curve.get(ps),     Interval(s.f, s.t),
                                          other.curve.get(ps), Interval(other.f, other.t));
            if(xs.empty()) continue;
            Crossing x = *std::min_element(xs.begin(), xs.end(), CrossingOrder(0, s.f > s.t));
            
            //draw_cross(cr, s.curve.get(ps)(x.ta));
            //draw_cross(cr, other.curve.get(ps)(x.tb));
            draw_line_seg(cr, s.curve.get(ps)(x.ta), other.curve.get(ps)(x.tb));
            cairo_stroke(cr);
            
            assert(Interval(s.f, s.t).contains(x.ta));
            assert(Interval(other.f, other.t).contains(x.tb));
            
            //split sections when necessary
            if(!(are_near(x.ta, s.f) || are_near(x.ta, s.t))) {
                Section sect = Section(s.curve.get(ps),    X, s.curve,     x.ta, s.t, vert, s.tv);
                monos.insert(std::lower_bound(monos.begin(), monos.end(), sect, SectionSorter(&ps, X)), sect);
                context[seg_ix].set_to(s.curve.get(ps), X, x.ta, vert);
            }
            if(!(are_near(x.tb, other.f) || are_near(x.tb, other.t))) {
                Section oth = Section(other.curve.get(ps), X, other.curve, x.tb, other.t, vert, other.tv);
                monos.insert(std::lower_bound(monos.begin(), monos.end(), oth, SectionSorter(&ps, X)), oth);
                context[others[i]].set_to(other.curve.get(ps), X, x.tb, vert);
            }
            
            vert++;
        }
        
        contexts.push_back(context);
    }
    
    return contexts;
}

class SweepWindow: public Toy {
    vector<Path> path;
    std::vector<Toggle> toggles;
    PointHandle p;
    std::vector<colour> colours;
    virtual void draw(cairo_t *cr, std::ostringstream *notify, int width, int height, bool save, std::ostringstream *timer_stream) {
        //draw_toggles(cr, toggles);
        cairo_set_source_rgb(cr, 1, 0, 0);
        cairo_set_line_width(cr, 0.5);
        //cairo_path(cr, path);
        cairo_stroke(cr);
        
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, 2);
        
        std::vector<std::vector<Section> > contexts = sweep_window(cr, path);
        
        /*  // this is code to make the handle like the location of the sweepline.
        double v = p.pos[X];
        for(unsigned i = 0; i < contexts.size(); i++) {
            if(std::max_element(contexts[i].begin(), contexts[i].end(), SectionSorter(&path, X))->fp[X] >= v) {
               // if(i != 0) i--;
                for(unsigned j = 0; j < contexts[i].size(); j++) {
                    draw_section(cr, contexts[i][j], path);
                    cairo_stroke(cr);
                }
                *notify << i << endl;
                break;
            }
        }
        cairo_set_line_width(cr, 0.5);
        draw_line_seg(cr, Point(p.pos[X], 0), Point(p.pos[X], 1000));
        cairo_stroke(cr);
        */
        
        int cix = (int) p.pos[X] / 10;
        if(cix >= 0 && cix < contexts.size()) {
            while(colours.size() < contexts[cix].size()) {
                double c = colours.size();
                colours.push_back(colour::from_hsv(c, 1, 1, 0.75));
            }
            for(unsigned i = 0; i < contexts[cix].size(); i++) {
                cairo_set_source_rgba(cr, colours[i]);
                cairo_set_line_width(cr, (i%3)+1);
                draw_section(cr, contexts[cix][i], path);
                cairo_stroke(cr);
            }
        }

        
        //some marks to indicate section breaks
        for(unsigned i = 0; i < path.size(); i++) {
            for(unsigned j = 0; j < path[i].size(); j++) {
                //draw_cross(cr, path[i][j].initialPoint());
            }
        }
        
        *notify << cix << endl;
        
        Toy::draw(cr, notify, width, height, save,timer_stream);
    }

    void mouse_pressed(GdkEventButton* e) {
        toggle_events(toggles, e);
        Toy::mouse_pressed(e);
    }
    
    void key_hit(GdkEventKey* e) {
        if(e->keyval == 'a') p.pos[X] = 0;
        else if(e->keyval == '[') p.pos[X] -= 10;
        else if(e->keyval == ']') p.pos[X] += 10;
        if (p.pos[X] < 0) {
            p.pos[X] = 0;
        }
        redraw();
    }
    
    public:
    SweepWindow () {}
    void first_time(int argc, char** argv) {
        const char *path_name="sanitize_examples.svgd";
        if(argc > 1)
            path_name = argv[1];
        path = read_svgd(path_name);
        OptRect bounds = bounds_exact(path);
        if(bounds)
            path += Point(10,10)-bounds->min();
        toggles.push_back(Toggle("Intersect", true));
        toggles[0].bounds = Rect(Point(10,100), Point(100, 130));
        p = PointHandle(Point(100,300));
        handles.push_back(&p);
    }
};

int main(int argc, char **argv) {
    init(argc, argv, new SweepWindow());
    return 0;
}

/*
// Returns a vector of sections, breaking the path into monotonic parts
// The from / to parts of the sections are such that increasing t yields increaasing x or y
std::vector<Section> paths_sections(std::vector<Path> const &ps, Dim2 d) {
    std::vector<Section> ret;
    for(unsigned i = 0; i < ps.size(); i++) {
        for(unsigned j = 0; j < ps[i].size(); j++) {
            std::vector<double> splits = mono_splits(ps[i][j]);
            for(unsigned k = 1; k < splits.size(); k++)
               ret.push_back(Section(ps[i][j], d, CurveIx(i,j), splits[k-1], splits[k]);
        }
    }
    return ret;
}
*/

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