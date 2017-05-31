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

#include <vector>
#include <random>

#include <boost/polygon/point_data.hpp>
#include <boost/polygon/segment_data.hpp>
#include <boost/polygon/voronoi.hpp>

#include "dialog.h"

static const int tile_size = 50;

using Point = boost::polygon::point_data<int>;
using Segment = boost::polygon::segment_data<int>;
using Grid = std::vector<Point>;

static Grid generateGrid(int w, int h, int rnd) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(-rnd/2, rnd/2);

    Grid g;
    g.reserve(size_t(w) * size_t(h));

    for (int x = 0; x < w; ++x)
        for (int y = 0; y < h; ++y)
            g.emplace_back(tile_size*x + dis(gen) + tile_size/2,
                           tile_size*y + dis(gen) + tile_size/2);

    return g;
}

static void drawGrid(QGraphicsView *view, const Grid &g, double w, double h) {
    view->scene()->clear();

    QPen pen(Qt::black, 3);
    view->scene()->addRect(0.0, 0.0, tile_size * w, tile_size * h,
                           pen, QBrush(Qt::transparent));

    pen.setColor(Qt::red);
    for (const auto &p : g)
        view->scene()->addEllipse(p.x()-3 , p.y()-3, 6, 6, pen, QBrush(Qt::red));

    view->fitInView(0, 0, tile_size * w, tile_size * h, Qt::KeepAspectRatio);
}

static std::vector<QPolygonF> computeVoronoi(const Grid &g, int w, int h) {
    QPolygonF rect;
    rect.append({0.0, 0.0});
    rect.append({0.0, tile_size * double(h)});
    rect.append({tile_size * double(w), tile_size * double(h)});
    rect.append({tile_size * double(w), 0.0});

    // create and add segments to ensure that the diagram spans the whole rect
    std::vector<Segment> s;

    for (int x = -1; x <= w; ++x) {
        Point p1(tile_size*(x+0), -2*tile_size);
        Point p2(tile_size*(x+1), -2*tile_size);
        s.emplace_back(p1, p2);

        p1 = Point(tile_size*(x+0), (h+2) * tile_size);
        p2 = Point(tile_size*(x+1), (h+2) * tile_size);
        s.emplace_back(p1, p2);
    }

    for (int y = -1; y <= h; ++y) {
        Point p1(-2*tile_size, tile_size*(y+0));
        Point p2(-2*tile_size, tile_size*(y+1));
        s.emplace_back(p1, p2);

        p1 = Point((w+2) * tile_size, tile_size*(y+0));
        p2 = Point((w+2) * tile_size, tile_size*(y+1));
        s.emplace_back(p1, p2);
    }

    boost::polygon::voronoi_diagram<double> vd;
    boost::polygon::construct_voronoi(g.begin(), g.end(), s.begin(), s.end(), &vd);

    std::vector<QPolygonF> cells;

    for (auto &c : vd.cells()) {
        if (!c.contains_point())
            continue;

        auto e = c.incident_edge();
        QPolygonF poly;
        do {
            if (e->is_primary()) {
                if (e->is_finite())
                    poly.append(QPointF(e->vertex0()->x(), e->vertex0()->y()));
                else {
                   if (e->vertex0())
                      poly.append(QPointF(e->vertex0()->x(), e->vertex0()->y()));
                   else if (e->vertex1())
                      poly.append(QPointF(e->vertex1()->x(), e->vertex1()->y()));
                }
            }
            e = e->next();
        } while (e != c.incident_edge());

        cells.push_back(poly.intersected(rect));
    }

    return cells;
}

static void drawCells(QGraphicsView *view, const std::vector<QPolygonF> &cells) {
    QPen pen(Qt::green, 2);
    for (const auto &poly : cells)
        view->scene()->addPolygon(poly, pen, QBrush(Qt::transparent));
}

Dialog::Dialog(QWidget *parent)
    : QDialog(parent)
{
    m_w_spin = new QSpinBox(this);
    m_w_spin->setRange(5, 1000);
    m_w_spin->setValue(30);

    m_h_spin = new QSpinBox(this);
    m_h_spin->setRange(5, 1000);
    m_h_spin->setValue(20);

    m_rand_spin = new QSpinBox(this);
    m_rand_spin->setRange(0, tile_size - 1);
    m_rand_spin->setValue(tile_size / 10);

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
    hbox->addWidget(new QLabel(tr("Randomness")));
    hbox->addWidget(m_rand_spin);
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
    int w = m_w_spin->value();
    int h = m_h_spin->value();
    int r = m_rand_spin->value();

    auto grid = generateGrid(w, h, r);
    auto vd = computeVoronoi(grid, w, h);

    drawGrid(m_view, grid, w, h);
    drawCells(m_view, vd);
}
