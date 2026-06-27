#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QString>
#include <string.h>
#include <QtCharts>
#include <QDateTime>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public:
    void Chart_init(void);                  //初始化图表区域函数

    struct TempInfo{
        QString currentTime;
        float currentTmep;
    };

    struct HumiInfo{
        QString currentTime;
        float currentHumi;
    };

    QChart *chart;
    QSplineSeries *seriesTemp;
    QSplineSeries *seriesHumi;
    //声明timer
    QTimer *timerDrawLine;           //定时器绘图

private:
    Ui::Widget *ui;

    bool mIsOpen;                       //标记串口是否已经打开
    /*声明一个串口类的成员*/
    QSerialPort mSerialPort;
    QString mPortName;
    QString mBaudRate;
    QString mParity;
    QString mDataBits;
    QString mStopBits;

    /*定义串口接收和发送的命令,由串口接收或发送出去*/
    QString  LiangDuReceive;
    QString LiangDuReceiveJudg;
    QString  TempReceive;
    QString  TempReceiveJudg;
    int TempReceiveShow;
    QString  HumiReceive;
    QString  HumiReceiveJudg;
    int HumiReceiveShow;
    QString LEDON = "@LEDON\r\n";
    QString LEDOFF = "@LEDOFF\r\n";
    QString MusicPlay = "@MusicPlay\r\n";
    QString MusicStop = "@MusicStop\r\n";
    QString FengShanON = "@FengShanON\r\n";
    QString FengShanOFF = "@FengShanOFF\r\n";
    QString FengSu25 = "@FengSu25\r\n";
    QString FengSu50 = "@FengSu50\r\n";
    QString FengSu75 = "@FengSu75\r\n";
    QString FengSu100 = "@FengSu100\r\n";
    QString FengSuShow;

private slots:
    void on_Button_clicked(int istate);           //开启串口函数

    void on_pB_StartLED_clicked();                //开灯函数
    void on_pB_StopLED_clicked();                 //关灯函数
    void on_pB_StartMusic_clicked();              //播放音乐函数
    void on_pB_StopMusic_clicked();               //关闭音乐函数
    void on_pB_StartFengShan_clicked();           //开风扇函数
    void on_pB_StopFengShan_clicked();            //关风扇函数

    void on_SerialPort_readyRead();               //接收信号函数

    void drawLine();                              //动态绘图函数


    void on_pB_FengSu25_clicked();
    void on_pB_FengSu50_clicked();
    void on_pB_FengSu75_clicked();
    void on_pB_FengSu100_clicked();
};
#endif // WIDGET_H
