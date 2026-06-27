#include "tempshow.h"
#include "ui_tempshow.h"

int TempShow::currentTemp = 25;    //全局变量

TempShow::TempShow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TempShow)
{
    ui->setupUi(this);
    this->setWindowTitle("温度仪表盘");
}

TempShow::~TempShow()
{
    delete ui;
}

void TempShow::paintEvent(QPaintEvent *event)
{
    (void)(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);
    /*绘制仪表盘背景*/
    int W = this->geometry().width();    //背景的宽
    int H = this->geometry().height();    //背景的高
    QRect rect(0,0,W,H); //中间区域矩形
    //设置画笔
    QPen pen;
    pen.setWidth(3);
    pen.setColor(Qt::gray);
    pen.setStyle(Qt::SolidLine);
    pen.setCapStyle(Qt::FlatCap);        //端点样式
    pen.setJoinStyle(Qt::BevelJoin);     //连接样式
    painter.setPen(pen);
    QColor color1(232, 254, 229);
    painter.drawRect(rect);              //绘制矩形
    painter.fillRect(rect,color1);   //填充背景颜色
    /*绘制表盘*/
    painter.translate(W/2,(H-23)/2+23);                                                           //将画笔移动到窗口界面大概一半位置，绘制仪表盘，圆心
    QRadialGradient tempRa(0,0,qMin(W/2,(H-23)/2));                                               //绘制一个带渐变的圆
    tempRa.setColorAt(0,Qt::green);                                                               //设置起点颜色
    tempRa.setColorAt(1,Qt::cyan);                                                                //设置终点颜色
    painter.setBrush(tempRa);                                                                     //设置画刷属性
    painter.setPen(Qt::NoPen);                                                                    //扔掉画笔
    painter.drawEllipse(QPoint(0,0),qMin(W/2,(H-23)/2),qMin(W/2,(H-23)/2)); //绘制一个圆

    /*绘制刻度线*/
    qreal angle = 270*1.0/(tempMaxValue-tempMinValue);                                            //计算刻度线间距
    painter.rotate(135);                                                                          //坐标系旋转135°
    painter.setPen(QPen(Qt::white,4));                                                            //设置画笔颜色和宽度
    painter.setFont(QFont("微软雅黑",10));                                                         //设置刻度文字字体和大小
    for(int i = tempMinValue;i<=tempMaxValue;++i)
    {
        if(i%10 == 0)
        {
            painter.drawText(qMin(W/2,(H-23)/2)-45,0+5,QString::number(i));                                           //在对应位置添加关键刻度文字
            painter.drawLine(qMin(W/2,(H-23)/2)-20,0,qMin(W/2,(H-23)/2),0);                                         //10的倍数的刻度画长一些
        }
        else if(i%2 == 0)
        {
            painter.drawLine(qMin(W/2,(H-23)/2)-10,0,qMin(W/2,(H-23)/2),0);   //非10的倍数的刻度画短一些
        }
        painter.rotate(angle);          //旋转坐标系
    }

    /*在表盘中心添加当前温度值*/
    painter.rotate(-angle-45);
    painter.drawEllipse(QPoint(0,0),qMin(W/2,(H-23)/2)-85,qMin(W/2,(H-23)/2)-85);
    painter.setPen(QPen(Qt::blue,4));
    painter.setFont(QFont("微软雅黑",32));
    painter.drawText(QRect(-100,-100,200,200),Qt::AlignCenter,QString::number(currentTemp));

    /*绘制指针*/
    painter.setPen(QPen(Qt::white,4));
    painter.rotate(135+(currentTemp-tempMinValue)*angle);
    painter.drawLine(qMin(W/2,(H-23)/2)-85,0,qMin(W/2,(H-23)/2)-40,0);
}
