#include <QTime>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsEllipseItem>

#include <cmath>
#include <vector>
#include <random>
#include <limits>

#include <boost/polygon/point_data.hpp>
#include <boost/polygon/segment_data.hpp>
#include <boost/polygon/voronoi.hpp>

#include "dialog.h"
#include "poisson-grid.h"

// scaling factor used to convert floating points coordinates into temporary
// integer coordinates to accomodate boost polygon, which sucks with floating points.
static const int SCALE = 100;

using Point = boost::polygon::point_data<int>;
using Segment = boost::polygon::segment_data<int>;
using Grid = std::vector<Point>;

static QColor colorAt(size_t i) {
    static const QColor colors[] = {
        QColor(252, 233,  79), QColor(237, 212,   0), QColor(196, 160,   0),
        QColor(138, 226,  52), QColor(115, 210,  22), QColor( 78, 154,   6),
        QColor(252, 175,  62), QColor(245, 121,   0), QColor(206,  92,   0),
        QColor(114, 159, 207), QColor( 52, 101, 164), QColor( 32,  74, 135),
        QColor(173, 127, 168), QColor(117,  80, 123), QColor( 92,  53, 102),
        QColor(233, 185, 110), QColor(193, 125,  17), QColor(143,  89,   2),
        QColor(239,  41,  41), QColor(204,   0,   0), QColor(164,   0,   0),
        QColor(238, 238, 236), QColor(211, 215, 207), QColor(186, 189, 182),
        QColor(136, 138, 133), QColor( 85,  87,  83), QColor( 46,  52,  54),
        QColor(  0,   0,   0)
    };

    return colors[i % (sizeof(colors)/sizeof(QColor))];
}

static Grid generateGrid(int w, int h, int num) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-0.75, 0.75);

    Grid g;
    g.reserve(size_t(w) * size_t(h));

    int seed = 0;
    PoissonGenerator::DefaultPRNG PRNG(seed);
    const auto points = PoissonGenerator::GeneratePoissonPoints(num, PRNG, 30, false);

    for(const auto &p : points) {
        double x = double(p.x) * w + dis(gen);
        if (x > w)
            x = w;
        if (x < 0)
            x = 0;
        double y = double(p.y) * h + dis(gen);
        if (y > h)
            y = h;
        if (y < 0)
            y = 0;
        g.emplace_back(SCALE*x, SCALE*y);
    }

    return g;
}

static void drawGrid(QGraphicsView *view, const Grid &g, double w, double h) {
    view->scene()->clear();

    QPen pen(Qt::black, 0.1);
    view->scene()->addRect(0.0, 0.0, w, h,
                           pen, QBrush(Qt::transparent));

    for (size_t i = 0; i < g.size(); ++i) {
        pen.setColor(colorAt(i));
        view->scene()->addEllipse(double(g[i].x())/SCALE -0.1,
                                  double(g[i].y())/SCALE -0.1,
                                  0.2, 0.2, pen, QBrush(colorAt(i)));
    }

    view->fitInView(0, 0, w, h, Qt::KeepAspectRatio);
}

static std::vector<QPolygonF> computeVoronoi(const Grid &g, double w, double h) {
    QPolygonF rect;
    rect.append({0.0, 0.0});
    rect.append({0.0, SCALE*h});
    rect.append({SCALE*w, SCALE*h});
    rect.append({SCALE*w, 0.0});

    boost::polygon::voronoi_diagram<double> vd;
    boost::polygon::construct_voronoi(g.begin(), g.end(), &vd);

    std::vector<QPolygonF> cells(g.size());

    for (auto &c : vd.cells()) {
        auto e = c.incident_edge();
        QPolygonF poly;
        do {
            if (e->is_primary()) {
                if (e->is_finite()) {
                    poly.append(QPointF(e->vertex0()->x(), e->vertex0()->y()));
                }
                else {
                    const auto &cell1 = e->cell();
                    const auto &cell2 = e->twin()->cell();
                    auto p1 = g[cell1->source_index()];
                    auto p2 = g[cell2->source_index()];
                    double ox = 0.5 * (p1.x() + p2.x());
                    double oy = 0.5 * (p1.y() + p2.y());
                    double dx = p1.y() - p2.y();
                    double dy = p2.x() - p1.x();
                    double coef = SCALE * w / std::max(fabs(dx), fabs(dy));

                    if (e->vertex0())
                        poly.append(QPointF(e->vertex0()->x(), e->vertex0()->y()));
                    else
                        poly.append(QPointF(ox - dx * coef, oy - dy * coef));

                    if (e->vertex1())
                        poly.append(QPointF(e->vertex1()->x(), e->vertex1()->y()));
                    else
                        poly.append(QPointF(ox + dx * coef, oy + dy * coef));
                }
            }
            e = e->next();
        } while (e != c.incident_edge());

        poly = poly.intersected(rect);
        for (auto &p : poly)
            p /= SCALE;

        cells[c.source_index()] = poly;
    }

    return cells;
}

static void drawCells(QGraphicsView *view, const std::vector<QPolygonF> &cells) {
    QPen pen(Qt::transparent, 0);
    for (size_t i = 0; i < cells.size(); ++i) {
        auto col = colorAt(i);
        col.setAlphaF(0.5);
        view->scene()->addPolygon(cells[i], pen, QBrush(col));
    }
}

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
{
    m_w_spin = new QSpinBox(this);
    m_w_spin->setRange(5, 1000);
    m_w_spin->setValue(44);

    m_h_spin = new QSpinBox(this);
    m_h_spin->setRange(5, 1000);
    m_h_spin->setValue(44);

    m_num_spin = new QSpinBox(this);
    m_num_spin->setRange(10, 10000);
    m_num_spin->setValue(1000);

    auto update = new QPushButton(tr("Update"), this);
    connect(update, SIGNAL(clicked()), SLOT(updateVoronoi()));

    auto hbox = new QHBoxLayout;
    hbox->addWidget(update);
    hbox->addSpacing(10);
    hbox->addWidget(new QLabel(tr("Width")));
    hbox->addWidget(m_w_spin);
    hbox->addSpacing(10);
    hbox->addWidget(new QLabel(tr("Height")));
    hbox->addWidget(m_h_spin);
    hbox->addSpacing(10);
    hbox->addWidget(new QLabel(tr("Number of points")));
    hbox->addWidget(m_num_spin);
    hbox->addStretch(1);

    auto scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(this);
    m_view->setScene(scene);

    auto vbox = new QVBoxLayout(this);
    vbox->addLayout(hbox);
    vbox->addWidget(m_view, 1);
    resize(1280, 800);
}

Dialog::~Dialog() {}

void Dialog::updateVoronoi() {
    QTime t;
    int w = m_w_spin->value();
    int h = m_h_spin->value();
    int n = m_num_spin->value();

    t.start();
    auto grid = generateGrid(w, h, n);
    qDebug("generateGrid took %d ms", t.elapsed());

    t.start();
    auto vd = computeVoronoi(grid, w, h);
    qDebug("computVoronoi took %d ms", t.elapsed());

    drawGrid(m_view, grid, w, h);
    drawCells(m_view, vd);
}
