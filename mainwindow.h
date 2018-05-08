#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/video.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/calib3d/calib3d.hpp>

#include <rcdraw.h>
#include <QFileDialog>


using namespace cv;

namespace Ui {
    class MainWindow;
}

struct pointHarris{
    Point p;
    float HarrisValue;
};

bool compareHarris(pointHarris a, pointHarris b);

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    struct region{
        int id;
        Point first;
        int numPoints;
        uchar gray;
        std::vector<Point> frontier;
    };

    struct pair{
        float bordersCanny;
        float frontier;
    };



    Ui::MainWindow *ui;
    QTimer timer;

    RCDraw *visorS, *visorD;
    QImage *imgS, *imgD;
    RCDraw *visorS2, *visorD2;
    QImage *imgS2, *imgD2;
    Mat colorImage, grayImage, destColorImage, destGrayImage, imageS2, imageD2;
    Mat gray2ColorImage, destGray2ColorImage;
    Mat regions, borders;
    Mat cornersLeft;
    Mat fixedPoints;
    std::vector<region> regionsList;
    std::vector<QMap<int,pair>> maps;
    std::vector<pointHarris> HarrisList;

public slots:
    void compute();
    void load_image();
    void segmentation_image();
    void create_region(Point inicial, int numberRegion);
    void draw_borders();
    void find_borders();
    void merge();

    void initDisparity_image();
    void propDisparity_image();
    void find_corners();
};


#endif // MAINWINDOW_H
