#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
class QSpinBox;
class QGraphicsView;
QT_END_NAMESPACE

class Dialog : public QDialog {
    Q_OBJECT

public:
    Dialog(QWidget *parent = 0);
    ~Dialog();

public slots:
    void updateVoronoi();

private:
    QSpinBox *m_w_spin, *m_h_spin, *m_rand_spin;
    QGraphicsView *m_view;
};

#endif // DIALOG_H
