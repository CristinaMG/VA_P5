#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    imgS = new QImage(320,240, QImage::Format_RGB888);
    visorS = new RCDraw(320,240, imgS, ui->imageFrameS);
    imgD = new QImage(320,240, QImage::Format_RGB888);
    visorD = new RCDraw(320,240, imgD, ui->imageFrameD);

    imgS2 = new QImage(320,240, QImage::Format_RGB888);
    visorS2 = new RCDraw(320,240, imgS2, ui->imageFrameS2);
    imgD2 = new QImage(320,240, QImage::Format_RGB888);
    visorD2 = new RCDraw(320,240, imgD2, ui->imageFrameD2);

    colorImage.create(240,320,CV_8UC3);
    grayImage.create(240,320,CV_8UC1);
    dImage.create(240,320,CV_8UC1);
    imageS2ColorImage.create(240,320,CV_8UC3);
    imageD2ColorImage.create(240,320,CV_8UC3);
    destGrayImage.create(240,320,CV_8UC1);
    gray2ColorImage.create(240,320,CV_8UC3);
    destGray2ColorImage.create(240,320,CV_8UC3);
    imageS2.create(240,320,CV_8UC1);
    imageD2.create(240,320,CV_8UC1);

    regions.create(240,320,CV_32SC1);

    cornersLeft.create(240,320,CV_32FC1);
    fixedPoints.create(240,320,CV_8UC1);

    disparity.create(240,320,CV_32FC1);

    connect(&timer,SIGNAL(timeout()),this,SLOT(compute()));

    connect(ui->loadButton,SIGNAL(clicked(bool)),this,SLOT(load_image()));
    connect(ui->initDButton,SIGNAL(clicked(bool)),this,SLOT(initDisparity_image()));

    connect(ui->loadGroundButton, SIGNAL(clicked(bool)),this, SLOT(load_image_disparity()));

    connect(visorS2, SIGNAL(windowSelected(QPointF,int,int)), this, SLOT(show_disparity(QPointF, int, int)));
    timer.start(60);

}

MainWindow::~MainWindow()
{
    delete ui;
    //delete cap;
    delete visorS;
    delete visorD;
    delete imgS;
    delete imgD;
    delete visorS2;
    delete visorD2;
    delete imgS2;
    delete imgD2;
}

void MainWindow::compute()
{
    maps.clear();

    if(ui->checkBoxCorners->isChecked()){
        draw_corners();
    }

    if(ui->propDButton->isChecked()){
        propDisparity_image();
    }

    draw_disparity();

    cvtColor(grayImage,gray2ColorImage, CV_GRAY2RGB);
    cvtColor(destGrayImage,destGray2ColorImage, CV_GRAY2RGB);
    cvtColor(imageS2,imageS2ColorImage, CV_GRAY2RGB);
    cvtColor(imageD2,imageD2ColorImage, CV_GRAY2RGB);


    memcpy(imgS->bits(), gray2ColorImage.data , 320*240*3*sizeof(uchar));
    memcpy(imgD->bits(), destGray2ColorImage.data , 320*240*3*sizeof(uchar));
    memcpy(imgS2->bits(), imageS2ColorImage.data , 320*240*3*sizeof(uchar));
    memcpy(imgD2->bits(), imageD2ColorImage.data , 320*240*3*sizeof(uchar));

    visorS->update();
    visorD->update();
    visorS2->update();
    visorD2->update();
}


void MainWindow::load_image()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
           tr("Open File"), "",
           tr("All Files (*)"));

    if (files.size() < 1)
           return;
    else {
           colorImage = imread(files[0].toStdString(), CV_LOAD_IMAGE_COLOR);
           width = colorImage.cols;
           cv::resize(colorImage, colorImage, Size(320,240));
           cvtColor(colorImage, grayImage, CV_BGR2GRAY);

           colorImage = imread(files[1].toStdString(), CV_LOAD_IMAGE_COLOR);
           cv::resize(colorImage, colorImage, Size(320,240));
           cvtColor(colorImage, destGrayImage, CV_BGR2GRAY);

    }
}

void MainWindow::load_image_disparity(){
    QString file = QFileDialog::getOpenFileName(this,
           tr("Open File"), "",
           tr("All Files (*)"));

   dImage = imread(file.toStdString(), CV_LOAD_IMAGE_COLOR);
   cv::resize(dImage, dImage, Size(320,240));
   cvtColor(dImage, imageD2, CV_BGR2GRAY);
}


void MainWindow::show_disparity(QPointF p, int w, int h){
    ui->lcdNumber->display(imageS2.at<uchar>(p.y(), p.x()));
    ui->lcdNumber_2->display(imageD2.at<uchar>(p.y(), p.x()));
}

void MainWindow::segmentation_image()
{
    int numberRegions = 0;
    uchar valueGray = 0;

    Canny(grayImage, borders, 40, 120);

    for(int i = 0; i<grayImage.rows; i++){
        for(int j = 0; j<grayImage.cols; j++){
            if(regions.at<int>(i,j) == -1 && borders.at<uchar>(i, j) == 0){
                create_region(Point(j,i), numberRegions);
                numberRegions++;

            }
        }

    }

    int min, reg;
    for(int i = 0; i<grayImage.rows; i++){
        for(int j = 0; j<grayImage.cols; j++){
            if(regions.at<int>(i,j) == -1){

                valueGray = grayImage.at<uchar>(i,j);
                min = 1000, reg = -1;
                for(int k = i-1; k<i+2; k++){
                    for(int l = j-1; l<j+2; l++){
                        if(l>=0 && k>=0 && l<grayImage.cols && k<grayImage.rows
                                && min > abs(valueGray - grayImage.at<uchar>(k, l))
                                && regions.at<int>(k, l)!= -1
                                && (l!=j || k!=i)){
                            min = abs(valueGray - grayImage.at<uchar>(k, l));
                            reg = regions.at<int>(k, l);
                        }
                    }
                }
                regions.at<int>(i,j) = reg;
                regionsList.at(reg).numPoints++;
            }
        }
    }
}

void MainWindow::create_region(Point inicial, int numberRegion){
    std::vector<Point> list;
    Point pAct;
    uint i =0;
    list.push_back(inicial);
    float av = 0.0, avNew = 0.0, dt = 0.0, dtNew = 0.0;
    int cont = 0;

    while(i<list.size()){
        pAct = list[i];

        if(pAct.x >=0 && pAct.x<grayImage.cols && pAct.y>=0 && pAct.y<grayImage.rows && regions.at<int>(pAct) == -1){
            avNew = (av * cont + grayImage.at<uchar>(pAct))/(cont+1);

            dtNew = dt + (grayImage.at<uchar>(pAct)-av)*(grayImage.at<uchar>(pAct)-avNew);

            if(sqrt(dtNew/(cont+1)) < 20.0)
            {
                av = avNew;
                dt = dtNew;
                cont++;
                regions.at<int>(pAct.y, pAct.x) = numberRegion;
                if(borders.at<uchar>(pAct)==0){
                    list.push_back(Point(pAct.x-1, pAct.y));
                    list.push_back(Point(pAct.x, pAct.y-1));
                    list.push_back(Point(pAct.x+1, pAct.y));
                    list.push_back(Point(pAct.x, pAct.y+1));
                }
            }
        }
        i++;
    }

    region region;
    region.first = inicial;
    region.gray = grayImage.at<uchar>(inicial);
    region.id = numberRegion;
    region.numPoints = list.size();
    region.numFixedPoints = 0;
    region.avgDisparity = 0.0;
    regionsList.push_back(region);
    list.clear();

}


void MainWindow::find_borders(){
    bool border = false;
    int id , idN;

    maps.clear();

    for(uint i = 0; i<regionsList.size();i++){
        maps.push_back(QMap<int, pair>());
    }

    for (uint var = 0; var < regionsList.size(); ++var) {
        regionsList[var].frontier.clear();
    }

    for(int i = 0; i<grayImage.rows; i++){
        for(int j = 0; j<grayImage.cols; j++){
            border = false;
            id=regions.at<int>(i,j);

            if(i>0 && regions.at<int>(i-1, j) != id){
                idN=regions.at<int>(i-1, j);
                regionsList.at(id).frontier.push_back(Point(j,i));
                border = true;

                if(maps.at(id).find(idN) == maps.at(id).end()){
                    maps.at(id)[idN].bordersCanny = 0;
                    maps.at(id)[idN].frontier = 0;
                }
                maps.at(id)[idN].frontier++;
                if(borders.at<uchar>(i,j) == 255 || borders.at<uchar>(i-1,j) ==255)
                    maps.at(id)[idN].bordersCanny++;
            }

            if(j>0 && regions.at<int>(i, j-1) != id && !border){
                idN=regions.at<int>(i, j-1);
                regionsList.at(id).frontier.push_back(Point(j,i));
                border = true;

                if(maps.at(id).find(idN) == maps.at(id).end()){
                    maps.at(id)[idN].bordersCanny = 0;
                    maps.at(id)[idN].frontier = 0;
                }
                maps.at(id)[idN].frontier++;
                if(borders.at<uchar>(i,j) == 255 || borders.at<uchar>(i,j-1) ==255)
                    maps.at(id)[idN].bordersCanny++;
            }

            if(i<grayImage.rows-1 && regions.at<int>(i+1,j) != id && !border){
                idN=regions.at<int>(i+1, j);
                regionsList.at(id).frontier.push_back(Point(j,i));
                border = true;

                if(maps.at(id).find(idN) == maps.at(id).end()){
                    maps.at(id)[idN].bordersCanny = 0;
                    maps.at(id)[idN].frontier = 0;
                }
                maps.at(id)[idN].frontier++;
                if(borders.at<uchar>(i,j) == 255 || borders.at<uchar>(i+1,j) ==255)
                    maps.at(id)[idN].bordersCanny++;
            }

            if(j<grayImage.cols-1 && regions.at<int>(i,j+1) != id && !border){
                idN=regions.at<int>(i, j+1);
                regionsList.at(id).frontier.push_back(Point(j,i));
                border = true;

                if(maps[id].find(idN) == maps.at(id).end()){
                    maps.at(id)[idN].bordersCanny = 0;
                    maps.at(id)[idN].frontier = 0;
                }
                maps.at(id)[idN].frontier++;
                if(borders.at<uchar>(i,j) == 255 || borders.at<uchar>(i,j+1) ==255)
                    maps.at(id)[idN].bordersCanny++;
            }
        }
    }
}

void MainWindow::merge(){
    float numFrontier = 0.0;
    bool merged = false;

    for(uint i = 0; i<regionsList.size(); i++){
        if(regionsList[i].id != -1){
            merged = false;
            for (uint j = i; j < regionsList.size() && !merged; ++j) {
                if(maps[i].find(j) != maps[i].end()){
                    numFrontier = 0;
                    if(regionsList[i].numPoints < regionsList[j].numPoints){
                        for(auto k : maps[i].keys()){
                            numFrontier += maps[i][k].frontier;
                        }
                    }else{
                        for(auto k : maps[j].keys()){
                            numFrontier += maps[j][k].frontier;
                        }
                    }

                    if(maps[i][j].frontier / numFrontier * 100.0 < 20.0){
                        if(maps[i][j].bordersCanny / maps[i][j].frontier * 100.0 < 40.0){ //Unir
                            merged = true;
                            for(int l=0; l <regions.rows; l++){
                                for(int m=0; m <regions.cols; m++){
                                    if(regions.at<int>(l,m) == j){
                                        regions.at<int>(l,m) = i;
                                    }
                                }
                            }
                            regionsList[j].id=-1;
                        }
                    }
                }
            }
        }
    }
    find_borders();
}


void MainWindow::initDisparity_image(){
    regions.setTo(-1);
    regionsList.clear();
    segmentation_image();
    find_borders();
    merge();
    find_corners();
}

void MainWindow::propDisparity_image(){
    for(int k = 3; k<fixedPoints.rows-3; k++){
        for(int l = 3; l<fixedPoints.cols-3; l++){
            if(fixedPoints.at<uchar>(k,l) == 0){
                float avg = 0.0;
                float cont = 0;
                for(int i = k-3; i<= k +3 ; i++){
                    for(int j = l-3; j<=l+3; j++){

                        if(regions.at<int>(k,l) == regions.at<int>(i,j)){
                            avg = (avg * cont + disparity.at<float>(i,j)) / (cont + 1.);
                            cont++;
                        }
                    }
                }
                disparity.at<float>(k, l) = avg;
            }
        }
    }
}


bool compareHarris(pointHarris a, pointHarris b){return a.HarrisValue>b.HarrisValue;}


void MainWindow::find_corners(){

    fixedPoints.setTo(0);
    disparity.setTo(0);
    imageS2.setTo(0);

    cornerHarris(grayImage, cornersLeft, 5, 3, 0.04);

    for(int i = 0; i<cornersLeft.rows; i++){
        for(int j = 0; j<cornersLeft.cols; j++){
            if(cornersLeft.at<float>(i,j)>=0.000001){
                pointHarris pH;
                pH.HarrisValue = cornersLeft.at<float>(i,j);
                pH.p = Point(j,i);
                HarrisList.push_back(pH);
            }
        }
    }

    std::sort(HarrisList.begin(), HarrisList.end(), compareHarris);

    for(uint k = 0; k<HarrisList.size(); k++){
        for(uint l = k+1; l < HarrisList.size(); l++){
            //Euclidean distance
            float eDist = sqrt(pow((HarrisList[k].p.x - HarrisList[l].p.x), 2) + pow((HarrisList[k].p.y - HarrisList[l].p.y),2));
            if(eDist < 5){
                HarrisList.erase(HarrisList.begin()+l);
            }
        }
    }


    for(uint m = 0; m<HarrisList.size(); m++){
        if(HarrisList[m].p.x >= WIN/2 && HarrisList[m].p.x < 320-WIN/2 && HarrisList[m].p.y >= WIN/2 && HarrisList[m].p.y <240-WIN/2){
            //Comprobación de la imagen izda a la dcha
            Mat stripeR = destGrayImage(cv::Rect(0, HarrisList[m].p.y-WIN/2, HarrisList[m].p.x, WIN));
            Mat patchL = grayImage(cv::Rect(HarrisList[m].p.x-WIN/2, HarrisList[m].p.y-WIN/2, WIN, WIN));
            Mat result;
            matchTemplate(stripeR, patchL, result, CV_TM_CCOEFF_NORMED);
            double min, max;
            Point pMin, pMax;
            minMaxLoc(result, &min, &max, &pMin, &pMax);

            if(max>0.9){
                //Comprobación de la imagen dcha a la izda
                Mat stripeL = grayImage(cv::Rect(0, HarrisList[m].p.y-WIN/2, 320, WIN));
                Mat patchR = destGrayImage(cv::Rect(pMax.x, HarrisList[m].p.y-WIN/2, WIN, WIN));
                Mat result2;
                matchTemplate(stripeL, patchR, result2, CV_TM_CCOEFF_NORMED);
                double min2, max2;
                Point pMin2, pMax2;
                minMaxLoc(result2, &min2, &max2, &pMin2, &pMax2);

                if(abs((pMax2.x+WIN/2)-HarrisList[m].p.x) < 4){

                    fixedPoints.at<uchar>(HarrisList[m].p.y, HarrisList[m].p.x) = 1;
                    disparity.at<float>(HarrisList[m].p.y, HarrisList[m].p.x) = HarrisList[m].p.x - (pMax.x + WIN/2);

                    float avg = regionsList[regions.at<int>(HarrisList[m].p.y, HarrisList[m].p.x)].avgDisparity;
                    avg = (avg * regionsList[regions.at<int>(HarrisList[m].p.y, HarrisList[m].p.x)].numFixedPoints
                            + disparity.at<float>(HarrisList[m].p.y, HarrisList[m].p.x))/
                            (regionsList[regions.at<int>(HarrisList[m].p.y, HarrisList[m].p.x)].numFixedPoints + 1.);

                    //Se actualiza la disparidad media y el número de puntos fijos
                    regionsList[regions.at<int>(HarrisList[m].p.y, HarrisList[m].p.x)].avgDisparity = avg;
                    regionsList[regions.at<int>(HarrisList[m].p.y, HarrisList[m].p.x)].numFixedPoints++;
                }
            }
        }
    }

    //Asignar disparidad a todos los puntos que no tenían
    for(int i = 0; i< disparity.rows; i++){
        for(int j = 0; j<disparity.cols; j++){
            if(fixedPoints.at<uchar>(i,j)==0){
                disparity.at<float>(i,j) = regionsList[regions.at<int>(i,j)].avgDisparity;
            }
        }
    }
}

void MainWindow::draw_corners(){
    for(uint i = 0; i<HarrisList.size();i++){
        if(fixedPoints.at<uchar>(HarrisList[i].p.y, HarrisList[i].p.x) == 0)
            visorS->drawSquare(QPointF(HarrisList[i].p.x,HarrisList[i].p.y), 2,2, Qt::red);
        else{
            visorS->drawSquare(QPointF(HarrisList[i].p.x,HarrisList[i].p.y), 2,2, Qt::green);
            int xR = HarrisList[i].p.x - disparity.at<float>(HarrisList[i].p.y, HarrisList[i].p.x);
            visorD->drawSquare(QPointF(xR ,HarrisList[i].p.y), 2,2, Qt::green);
        }
    }
}

void MainWindow::draw_disparity(){
    for(int i = 0; i< imageS2.rows; i++){
        for(int j = 0; j< imageS2.cols; j++){
            int gray = disparity.at<float>(i,j) * 3. * width/320.;
            if(gray>255)
                gray = 255;
            if(gray<0)
                gray = 0;
            imageS2.at<uchar>(i,j) = gray;
        }
    }
}
