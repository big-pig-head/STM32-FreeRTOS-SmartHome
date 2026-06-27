#ifndef TEMPSHOW_H
#define TEMPSHOW_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class TempShow;
}

class TempShow : public QWidget
{
    Q_OBJECT

public:
    explicit TempShow(QWidget *parent = nullptr);
    ~TempShow();


private:
    Ui::TempShow *ui;
    
    void paintEvent(QPaintEvent *event) override;

private:
    int tempMaxValue = 60;                          //气温最高值为60℃
    int tempMinValue = -50;                         //气温最低值-50℃
public:
    static int currentTemp;                           //初始化当前温度为25℃
};

#endif // TEMPSHOW_H
