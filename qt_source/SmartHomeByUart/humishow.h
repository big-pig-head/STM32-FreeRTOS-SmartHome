#ifndef HUMISHOW_H
#define HUMISHOW_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class humishow;
}

class humishow : public QWidget
{
    Q_OBJECT


public:
    explicit humishow(QWidget *parent = nullptr);
    ~humishow();


private:

    Ui::humishow *ui;
    void paintEvent(QPaintEvent *event) override;

private:
    int humiMaxValue = 100;                       //湿度最高值为100%
    int humiMinValue = 0;                         //湿度最低值0%

public:
    static int currenthumi;                  //当前湿度


};

#endif // HUMISHOW_H
