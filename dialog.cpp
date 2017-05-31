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

using Point = boost::polygon::point_data<double>;
using Segment = boost::polygon::segment_data<double>;
using Grid = std::vector<Point>;


static Grid generateGrid(int w, int h, int num) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-0.75, 0.75);

    Grid g;
    g.reserve(size_t(w) * size_t(h));

    int seed = 0;
    PoissonGenerator::DefaultPRNG PRNG(seed);
    const auto points = PoissonGenerator::GeneratePoissonPoints(num, PRNG, num, false);
    qDebug("Size: %ld", points.size());

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
        g.emplace_back(x, y);
    }

    return g;
}

static void drawGrid(QGraphicsView *view, const Grid &g, double w, double h) {
    view->scene()->clear();

    QPen pen(Qt::black, 0.1);
    view->scene()->addRect(0.0, 0.0, w, h,
                           pen, QBrush(Qt::transparent));

    pen.setColor(Qt::red);
    for (const auto &p : g)
        view->scene()->addEllipse(p.x() -0.05 , p.y() -0.05, 0.1, 0.1, pen, QBrush(Qt::red));

    view->fitInView(0, 0, w, h, Qt::KeepAspectRatio);
}

static std::vector<QPolygonF> computeVoronoi(const Grid &g, double w, double h) {
    QPolygonF rect;
    rect.append({0.0, 0.0});
    rect.append({0.0, h});
    rect.append({w, h});
    rect.append({w, 0.0});

    boost::polygon::voronoi_diagram<double> vd;
    boost::polygon::construct_voronoi(g.begin(), g.end(), &vd);

    std::vector<QPolygonF> cells;
    cells.reserve(g.size());

    for (auto &c : vd.cells()) {
        if (!c.contains_point())
            continue;

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
                    Point orig(0.5 * (p1.x()+p2.x()), 0.5 * (p1.y()+p2.y()));
                    Point dir(p1.y() - p2.y(), p2.x() - p1.x());
                    double coef = w / std::max(fabs(dir.x()), fabs(dir.y()));

                    if (e->vertex0()) {
                        poly.append(QPointF(e->vertex0()->x(), e->vertex0()->y()));
                    }
                    else {
                        poly.append(QPointF(orig.x() - dir.x() * coef,
                                            orig.y() - dir.y() * coef));
                    }
                    if (e->vertex1()) {
                        poly.append(QPointF(e->vertex1()->x(), e->vertex1()->y()));
                    }
                    else {
                        poly.append(QPointF(orig.x() + dir.x() * coef,
                                            orig.y() + dir.y() * coef));
                    }
                }
            }
            e = e->next();
        } while (e != c.incident_edge());

        cells.push_back(poly.intersected(rect));
    }

    return cells;
}

static void drawCells(QGraphicsView *view, const std::vector<QPolygonF> &cells) {
    QPen pen(Qt::green, 0.05);
    for (const auto &poly : cells)
        view->scene()->addPolygon(poly, pen, QBrush(Qt::transparent));
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
    m_num_spin->setRange(10, 1000);
    m_num_spin->setValue(50);

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
    auto vd = computeVoronoi(grid, w, h);

    qDebug("(%d x %d) took %d ms", w, h, t.elapsed());

    drawGrid(m_view, grid, w, h);
    drawCells(m_view, vd);
}
