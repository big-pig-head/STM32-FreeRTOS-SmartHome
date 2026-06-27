#include "widget.h"
#include "ui_widget.h"
#include <QSerialPortInfo>
#include <QList>
#include <QString>
#include <QDebug>
#include <stdio.h>
#include <string.h>
#include "tempshow.h"
#include "humishow.h"
#include <QLayout>
#include <QLabel>
#include <QFont>
#include <QDateTime>
#include <QTime>
#include <QFile>
#include <iostream>
#include <QTextStream>
#include <QTextCodec>

#pragma execution_character_set("utf-8");

Widget::TempInfo *tempinfo = new Widget::TempInfo();                         //实例化一个温度信息结构体
Widget::HumiInfo *humiinfo = new  Widget::HumiInfo();                         //实例化一个湿度信息结构体

QFile tempInfofile("tempinfo.csv");        //温度数据采样文件
QFile humiInfofile("humiinfo.csv");       //湿度数据采样文件



Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    //User's Code
    this->setWindowTitle("智慧家居系统");
    TempShow *tempShow = new TempShow(this);                         //在主窗口中新建温度仪表盘类
    QRect tempPosition = this->ui->label_ShowTemp->geometry();      //获取主窗口中显示温度仪表盘的标签的位置和大小
    tempShow->setGeometry(tempPosition);                            //将温度仪表盘嵌入主窗口中

    humishow *humiShow = new humishow(this);                         //在主窗口中新建湿度仪表盘类
    QRect humiPosition = this->ui->label_ShowHumi->geometry();       //获取主窗口中显示湿度仪表盘的标签的位置和大小
    humiShow->setGeometry(humiPosition);                             //将湿度仪表盘嵌入主窗口中

    /*初始化串口设置区域*/
    //初始化串口设置选项
    mIsOpen = false;                    //初始化串口为关闭状态
    QList<QSerialPortInfo>SerialPortInfo = QSerialPortInfo::availablePorts();   //获取设备上可用串口并放入一个列表中
    int count = SerialPortInfo.count();
    for(int i = 0;i<count;i++)
    {
        ui->cB_SerialPort->addItem(SerialPortInfo.at(i).portName());            //返回第i个串口信息并放入串口复选框item中
    }

    //初始化打开串口复选框
    ui -> cB_StartAndStopPort->setCheckState(Qt::Unchecked); //初始化为串口复选框默认为为打开状态
    ui ->cB_StartAndStopPort->setText("打开串口");          //串口未打开时复选框显示“打开串口”
//    ui ->cB_StartAndStopPort->setTristate();             //不开启，开启后为三态
    /*初始化家电控制区域*/
    ui->label_ShowLiangDu->setEnabled(false);
    ui->pB_StartLED->setEnabled(false);
    ui->pB_StopLED->setEnabled(false);
    ui->pB_StartMusic->setEnabled(false);
    ui->pB_StopMusic->setEnabled(false);
    ui->pB_StartFengShan->setEnabled(false);
    ui->pB_StopFengShan->setEnabled(false);
    ui->pB_FengSu25->setEnabled(false);
    ui->pB_FengSu50->setEnabled(false);
    ui->pB_FengSu75->setEnabled(false);
    ui->pB_FengSu100->setEnabled(false);
    ui->label_ShowLiangDu->setText("");

    if (tempInfofile.open(QIODevice::Append | QIODevice::Text | QIODevice::WriteOnly))
    {
        // 创建文本流
        QTextStream tempStream(&tempInfofile);
        // 设置以追加模式写入
        tempStream.setAutoDetectUnicode(true);           //自动检测编码格式，否则会乱码
//        tempStream.setCodec("UTF-8");
        QString TimeTitle = "时间";
        QString TempTitle = "温度";
        tempStream << TimeTitle;              //要使用变量方式传递数据，否则会乱码
        tempStream <<',';
        tempStream <<TempTitle<<endl;
        tempInfofile.close();
    }

    if (humiInfofile.open(QIODevice::Append | QIODevice::Text | QIODevice::WriteOnly))
    {
        // 创建文本流
        QTextStream humiStream(&humiInfofile);
        // 设置以追加模式写入
        humiStream.setAutoDetectUnicode(true);           //自动检测编码格式，否则会乱码
//        humiStream.setCodec("UTF-8");
        QString TimeTitle = "时间";
        QString HumiTitle = "湿度";
        humiStream << TimeTitle;
        humiStream <<',';
        humiStream <<HumiTitle<<endl;
        humiInfofile.close();
    }

    //初始化折线图
    Chart_init();
    connect(ui->cB_StartAndStopPort,SIGNAL(stateChanged(int)),this,SLOT(on_Button_clicked(int)));   //打开串口
    connect(ui->pB_StartLED,SIGNAL(clicked(bool)),this,SLOT(on_pB_StartLED_clicked));               //开灯
    connect(ui->pB_StopLED,SIGNAL(clicked(bool)),this,SLOT(on_pB_StopLED_clicked));                 //关灯
    connect(ui->pB_StartMusic,SIGNAL(clicked(bool)),this,SLOT(on_pB_StartMusic_clicked));           //播放音乐
    connect(ui->pB_StopMusic,SIGNAL(clicked(bool)),this,SLOT(on_pB_StopMusic_clicked));             //关闭音乐
    connect(ui->pB_StartFengShan,SIGNAL(clicked(bool)),this,SLOT(on_pB_StartFengShan_clicked));     //开启风扇
    connect(ui->pB_StopFengShan,SIGNAL(clicked(bool)),this,SLOT(on_pB_StopFengShan_clicked));       //关闭风扇
    connect(ui->pB_FengSu25,SIGNAL(clicked(bool)),this,SLOT(on_pB_FengSu25_clicked));             //开启风扇,并设置风速为25%
    connect(ui->pB_FengSu50,SIGNAL(clicked(bool)),this,SLOT(on_pB_FengSu50_clicked));             //开启风扇,并设置风速为50%
    connect(ui->pB_FengSu75,SIGNAL(clicked(bool)),this,SLOT(on_pB_FengSu75_clicked));             //开启风扇,并设置风速为75%
    connect(ui->pB_FengSu100,SIGNAL(clicked(bool)),this,SLOT(on_pB_FengSu100_clicked));             //开启风扇,并设置风速为100%
    connect(&mSerialPort, SIGNAL(readyRead()), this, SLOT(on_SerialPort_readyRead()));               //数据接收函数
    connect(&mSerialPort, SIGNAL(readyRead()), this, SLOT(drawLine()));
    /*最后两个函数不知道为什么，不带括号不会执行，其余的发送信息的函数不能带括号，否则会发两遍*/
}

Widget::~Widget()
{
    delete ui;
}

/*创建绘图区域函数*/
void Widget::Chart_init()
{
    chart = new QChart;            //实例化一个图表对象
    chart->setTitle("温湿度曲线图");         //设置标题名称
    chart->setTitleFont(QFont("宋体",5,QFont::Black));   //设置图标标题文字大小
    seriesTemp = new QSplineSeries();           //实例化温度曲线
    seriesHumi = new QSplineSeries();           //实例化湿度曲线
    //设置曲线名称
    seriesTemp->setName("温度");
    seriesHumi->setName("湿度");
    chart->legend()->setFont(QFont("宋体",5,QFont::Black));
    //把曲线添加到图表中
    chart->addSeries(seriesTemp);
    chart->addSeries(seriesHumi);
    //声明并初始化x轴和两个Y轴
    QDateTimeAxis *axisX = new QDateTimeAxis();
    axisX->setFormat("yy/MM/dd hh:mm");
    QValueAxis *axisY_temp = new QValueAxis();
    QValueAxis *axisY_humi = new QValueAxis();
    //设置坐标轴名称
    axisX->setTitleText("时间");
    axisY_temp->setTitleText("温度(℃)");
    axisY_humi->setTitleText("湿度(%RH)");
    //设置坐标轴样式
    axisY_temp->setTitleFont(QFont("宋体",5,QFont::Black));
    axisY_humi->setTitleFont(QFont("宋体",5,QFont::Black));
    axisX->setLabelsAngle(75);                    //横坐标刻度标签很长，旋转80°
    axisX->setLabelsFont(QFont("宋体",5,QFont::Black));   //设置横坐标字体及大小
    axisY_temp->setLabelsFont(QFont("宋体",5,QFont::Black));   //设置横坐标字体及大小
    axisY_humi->setLabelsFont(QFont("宋体",5,QFont::Black));   //设置横坐标字体及大小
    //设置坐标轴范围
    axisX->setMin(QDateTime::currentDateTime().addSecs(-60 * 1));
    axisX->setMax(QDateTime::currentDateTime().addSecs(0));
    axisY_temp->setMin(-50);
    axisY_temp->setMax(60);
    axisY_humi->setMin(0);
    axisY_humi->setMax(100);
    //设置坐标轴上的刻度
    axisX->setTickCount(10);
    axisY_temp->setTickCount(11);
    axisY_humi->setTickCount(10);
    //设置坐标轴的颜色，粗细，设置网格不显示
    axisY_temp->setLinePenColor(QColor(Qt::darkBlue));
    axisY_temp->setGridLineColor(QColor(Qt::darkBlue));
    axisY_humi->setLinePenColor(QColor(Qt::darkGreen));
    axisY_humi->setGridLineColor(QColor(Qt::darkGreen));
    axisY_temp->setGridLineVisible(false);
    axisY_humi->setGridLineVisible(false);
    QPen penY1(Qt::blue,1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen penY2(Qt::green,1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    axisY_temp->setLinePen(penY1);
    axisY_humi->setLinePen(penY2);

    //把坐标轴添加到chart中，
    //addAxis函数的第二个参数是设置坐标轴的位置，
    //只有四个选项，下方：Qt::AlignBottom，左边：Qt::AlignLeft，右边：Qt::AlignRight，上方：Qt::AlignTop
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY_temp, Qt::AlignLeft);
    chart->addAxis(axisY_humi, Qt::AlignRight);

    //把曲线关联到坐标轴
    seriesTemp->attachAxis(axisX);
    seriesTemp->attachAxis(axisY_temp);
    seriesHumi->attachAxis(axisX);
    seriesHumi->attachAxis(axisY_humi);

    //把chart显示到窗口上
    ui->graphicsView->setChart(chart);
}


//实现画图函数，动态更新
void Widget::drawLine()
{
    //每增加一个点改变X轴的范围，实现曲线的动态更新效果
    QDateTime bjtime = QDateTime::currentDateTime();
    qsrand(QTime(0,0,0).secsTo(QTime::currentTime()));
    chart->axisX()->setMin(QDateTime::currentDateTime().addSecs(-60 * 1));
    chart->axisX()->setMax(QDateTime::currentDateTime().addSecs(0));
    //绘制坐标点
    if(tempinfo->currentTmep)
        seriesTemp->append(bjtime.toMSecsSinceEpoch(), tempinfo->currentTmep);
    if(humiinfo->currentHumi)
        seriesHumi->append(bjtime.toMSecsSinceEpoch(), humiinfo->currentHumi);
}


/*点击按钮，获取界面上的串口信息，并打开串口*/
void Widget::on_Button_clicked(int istate)
{
    if(istate)            //串口打开后，进制使用界面设置串口信息
    {
        //获取串口配置
        mPortName = ui->cB_SerialPort->currentText();           //获取串口编辑组合框
        mBaudRate = ui->cB_BaduRate->currentText();             //获取波特率编辑组合框
        mParity = ui ->cB_Parity->currentText();                //获取校验位编辑组合框
        mDataBits = ui->cB_DataBits->currentText();             //获取数据位编辑组合框
        mStopBits = ui->cB_StopBits->currentText();             //获取停止位编辑组合框

        //将从UI获取到的信息传递给串口对象
        //端口号
        mSerialPort.setPortName(mPortName);
        //波特率；
        if(mBaudRate == "9600")
            mSerialPort.setBaudRate(QSerialPort::Baud9600);
        else if(mBaudRate == "19200")
            mSerialPort.setBaudRate(QSerialPort::Baud19200);
        else
            mSerialPort.setBaudRate(QSerialPort::Baud115200);
        //校验位
        if(mParity == "Odd")
            mSerialPort.setParity(QSerialPort::OddParity);
        else if(mParity == "Even")
            mSerialPort.setParity(QSerialPort::EvenParity);
        else
            mSerialPort.setParity(QSerialPort::NoParity);
        //数据位
        if(mDataBits == "5")
            mSerialPort.setDataBits(QSerialPort::Data5);
        else if(mDataBits == "6")
            mSerialPort.setDataBits(QSerialPort::Data6);
        else if(mDataBits == "7")
            mSerialPort.setDataBits(QSerialPort::Data7);
        else
            mSerialPort.setDataBits(QSerialPort::Data8);
        //停止位
        if(mStopBits == "1.5")
            mSerialPort.setStopBits(QSerialPort::OneAndHalfStop);
        else if(mStopBits == "2")
            mSerialPort.setStopBits(QSerialPort::TwoStop);
        else
            mSerialPort.setStopBits(QSerialPort::OneStop);

        mSerialPort.open(QSerialPort::ReadWrite);      //复选框开时以读写方式打开串口
        qDebug()<<"成功打开串口"<<mPortName;
        mIsOpen = true;
        ui->cB_StartAndStopPort->setText("关闭串口");
        ui->cB_SerialPort->setEnabled(false);
        ui->cB_BaduRate->setEnabled(false);
        ui->cB_Parity->setEnabled(false);
        ui->cB_DataBits->setEnabled(false);
        ui->cB_StopBits->setEnabled(false);
        ui->label_ShowLiangDu->setEnabled(true);
        ui->pB_StartLED->setEnabled(true);
        ui->pB_StopLED->setEnabled(true);
        ui->pB_StartMusic->setEnabled(true);
        ui->pB_StopMusic->setEnabled(true);
        ui->pB_StartFengShan->setEnabled(true);
        ui->pB_StopFengShan->setEnabled(true);
        ui->pB_FengSu25->setEnabled(true);
        ui->pB_FengSu50->setEnabled(true);
        ui->pB_FengSu75->setEnabled(true);
        ui->pB_FengSu100->setEnabled(true);
    }
    else
    {
        mSerialPort.close();           //复选框未选中时候要执行关闭串口动作，同时相应设置变得可用
        qDebug()<<"关闭串口";
        mIsOpen = false;
        ui->cB_StartAndStopPort->setText("打开串口");
        ui->cB_SerialPort->setEnabled(true);
        ui->cB_BaduRate->setEnabled(true);
        ui->cB_Parity->setEnabled(true);
        ui->cB_DataBits->setEnabled(true);
        ui->cB_StopBits->setEnabled(true);
        //串口没打开时候家电控制选项均不可用
        ui->label_ShowLiangDu->setEnabled(false);
        ui->pB_StartLED->setEnabled(false);
        ui->pB_StopLED->setEnabled(false);
        ui->pB_StartMusic->setEnabled(false);
        ui->pB_StopMusic->setEnabled(false);
        ui->pB_StartFengShan->setEnabled(false);
        ui->pB_StopFengShan->setEnabled(false);
        ui->pB_FengSu25->setEnabled(false);
        ui->pB_FengSu50->setEnabled(false);
        ui->pB_FengSu75->setEnabled(false);
        ui->pB_FengSu100->setEnabled(false);
    }
}

/*发送开灯指令*/
void Widget::on_pB_StartLED_clicked()           //如果LED开按钮被点击，发送“LEDON”命令
{
    mSerialPort.write(LEDON.toStdString().c_str()); //Qt中默认的是QString格式，要先转换成标准的String格式，再转换成char *格式
    qDebug()<<"成功下发开灯指令"<<endl;
}

/*发送关灯指令*/
void Widget::on_pB_StopLED_clicked()
{
    mSerialPort.write(LEDOFF.toStdString().c_str());
    qDebug()<<"成功下发关灯指令"<<endl;
}

/*发送播放音乐指令*/
void Widget::on_pB_StartMusic_clicked()
{
    mSerialPort.write(MusicPlay.toStdString().c_str());
    qDebug()<<"成功下发播放音乐指令"<<endl;
}

/*发送停止播放音乐指令*/
void Widget::on_pB_StopMusic_clicked()
{
    mSerialPort.write(MusicStop.toStdString().c_str());
    qDebug()<<"成功下发停止播放音乐指令"<<endl;
}

/*打开风扇指令*/
void Widget::on_pB_StartFengShan_clicked()
{
    mSerialPort.write(FengShanON.toStdString().c_str());
    qDebug()<<"成功下发开风扇指令"<<endl;
}

/*关闭风扇指令*/
void Widget::on_pB_StopFengShan_clicked()
{
    mSerialPort.write(FengShanOFF.toStdString().c_str());
    qDebug()<<"成功下发关风扇指令"<<endl;
}

/*风速设置指令*/
void Widget::on_pB_FengSu25_clicked()
{
    mSerialPort.write(FengSu25.toStdString().c_str());
    mSerialPort.write("\n");
}

void Widget::on_pB_FengSu50_clicked()
{
    mSerialPort.write(FengSu50.toStdString().c_str());
    mSerialPort.write("\n");
}

void Widget::on_pB_FengSu75_clicked()
{
    mSerialPort.write(FengSu75.toStdString().c_str());
    mSerialPort.write("\n");
}

void Widget::on_pB_FengSu100_clicked()
{
    mSerialPort.write(FengSu100.toStdString().c_str());
    mSerialPort.write("\n");
}

/*接收信号槽函数*/
void Widget::on_SerialPort_readyRead()
{
    if(mIsOpen)              //如果串口打开，接收数据
    {
        QByteArray recvData = mSerialPort.readAll();          //读串口
        LiangDuReceiveJudg = recvData.mid(0,7);
        TempReceiveJudg = recvData.mid(0,4);
        HumiReceiveJudg = recvData.mid(0,4);

        if(LiangDuReceiveJudg == "LiangDu")
        {
            ui->label_ShowLiangDu->setText(recvData.mid(8,recvData.length()));
        }
        else
            ui->label_ShowLiangDu->setText("");

        if(TempReceiveJudg == "Temp")
        {
            qDebug()<<"读取到温度值:";
            TempReceiveShow = int(recvData.mid(5,recvData.length()).toFloat()+0.5);    //为了防止强制转换时候不进行四舍五入，在转换为整数前+0.5
            TempShow::currentTemp = TempReceiveShow;                          //更新温度值
            QDateTime curDateTime =  QDateTime::currentDateTime();            // 获取当前时间
            tempinfo->currentTime = curDateTime.toString("yyyy-MM-dd hh:mm:ss");
            tempinfo->currentTmep = recvData.mid(5,recvData.length()).toFloat();
            qDebug()<<tempinfo->currentTmep<<endl;
            if(tempinfo->currentTmep)
            {
                if (tempInfofile.open(QIODevice::Append | QIODevice::Text | QIODevice::WriteOnly))
                {
                    // 创建文本流
                    QTextStream tempStream(&tempInfofile);
                    // 设置以追加模式写入
    //                tempStream.setAutoDetectUnicode(true);
                    tempStream.setCodec(QTextCodec::codecForName("UTF-8"));
                    tempStream << tempinfo->currentTime;
                    tempStream <<',';
                    tempStream <<tempinfo->currentTmep<<endl;
                    tempInfofile.close();
                }
                this->update();          //这里需要刷新一下界面
            }
        }
        if(HumiReceiveJudg == "Humi")
        {
            qDebug()<<"读取到湿度值:";
            HumiReceiveShow = int(recvData.mid(5,recvData.length()).toFloat()+0.5);
            humishow::currenthumi = HumiReceiveShow;                          //更新湿度值
            QDateTime curDateTime =  QDateTime::currentDateTime();            // 获取当前时间
            humiinfo->currentTime = curDateTime.toString("yyyy-MM-dd hh:mm:ss");
            humiinfo->currentHumi = recvData.mid(5,recvData.length()).toFloat();
            qDebug()<<humiinfo->currentHumi<<endl;
            if(humiinfo->currentHumi)
            {
                if (humiInfofile.open(QIODevice::Append | QIODevice::Text | QIODevice::WriteOnly))
                {
                    // 创建文本流
                    QTextStream humiStream(&humiInfofile);
                    // 设置以追加模式写入
                    QTextCodec *codec = QTextCodec::codecForName("UTF-8");
                    humiStream.setCodec(codec);
                    humiStream << humiinfo->currentTime;
                    humiStream <<',';
                    humiStream <<humiinfo->currentHumi<<endl;
                    humiInfofile.close();
                }
                this->update();          //这里需要刷新一下界面
            }
        }

    }
}




