#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

using namespace std;
using namespace cv;

void show_dct(const char *image_file) {
    Mat rgbImage = imread(image_file);
    imshow("Origin-RGB", rgbImage);

    Mat yuvImage(rgbImage.size(), CV_8UC3);
    cvtColor(rgbImage, yuvImage, CV_BGR2YUV);

    vector<Mat> channels;
    split(yuvImage, channels);

    Mat y = channels.at(0);
    imshow("Y", y);
    Mat u = channels.at(1);
    imshow("U", u);
    Mat v = channels.at(2);
    imshow("V", v);

    Mat DCTY(rgbImage.size(), CV_64FC1);
    Mat DCTU(rgbImage.size(), CV_64FC1);
    Mat DCTV(rgbImage.size(), CV_64FC1);

    dct(Mat_<double>(y), DCTY);
    dct(Mat_<double>(u), DCTU);
    dct(Mat_<double>(v), DCTV);

    channels.at(0) = Mat_<uchar>(DCTY);
    channels.at(1) = Mat_<uchar>(DCTU);
    channels.at(2) = Mat_<uchar>(DCTV);

    Mat dstImage(rgbImage.size(), CV_64FC3);
    merge(channels, dstImage);
    imshow("DCT", dstImage);

    waitKey();
}

void show_dft(const char *image_file) {
    Mat I = imread(image_file, CV_LOAD_IMAGE_GRAYSCALE);

    //expand input image to optimal size
    Mat padded;
    int m = getOptimalDFTSize(I.rows);
    int n = getOptimalDFTSize(I.cols);
    copyMakeBorder(I, padded, 0, m - I.rows, 0, n - I.cols, BORDER_CONSTANT, Scalar::all(0));

    // Add to the expanded another plane with zeros
    Mat planes[] = {Mat_<float>(padded), Mat::zeros(padded.size(), CV_32F)};
    Mat complexI;
    merge(planes, 2, complexI);

    dft(complexI, complexI);

    // compute the magnitude and switch to logarithmic scale
    // => log(1 + sqrt(Re(DFT(I))^2 + Im(DFT(I))^2))
    split(complexI, planes);                    // planes[0] = Re(DFT(I), planes[1] = Im(DFT(I))
    magnitude(planes[0], planes[1], planes[0]); // planes[0] = magnitude
    Mat magI = planes[0];

    // switch to logarithmic scale
    magI += Scalar::all(1);
    log(magI, magI);

    // crop the spectrum, if it has an odd number of rows or columns
    magI = magI(Rect(0, 0, magI.cols & -2, magI.rows & -2));

    // rearrange the quadrants of Fourier image  so that the origin is at the image center
    int cx = magI.cols / 2;
    int cy = magI.rows / 2;

    Mat q0(magI, Rect(0, 0, cx, cy));   // Top-Left - Create a ROI per quadrant
    Mat q1(magI, Rect(cx, 0, cx, cy));  // Top-Right
    Mat q2(magI, Rect(0, cy, cx, cy));  // Bottom-Left
    Mat q3(magI, Rect(cx, cy, cx, cy)); // Bottom-Right

    // swap quadrants (Top-Left with Bottom-Right)
    Mat tmp;
    q0.copyTo(tmp);
    q3.copyTo(q0);
    tmp.copyTo(q3);

    // swap quadrant (Top-Right with Bottom-Left)
    q1.copyTo(tmp);
    q2.copyTo(q1);
    tmp.copyTo(q2);

    // Transform the matrix with float values into a
    // viewable image form (float between values 0 and 1).
    normalize(magI, magI, 0, 1, CV_MINMAX);

    imshow("Input Image", I);
    imshow("spectrum magnitude", magI);
    waitKey();
}

void show_hist(const char *image_file) {
    Mat src, dst;

    src = imread(image_file);

    /// Separate the image in 3 places ( B, G and R )
    vector<Mat> bgr_planes;
    split(src, bgr_planes);

    /// Establish the number of bins
    int histSize = 256;

    /// Set the ranges ( for B,G,R) )
    float range[] = {0, 256};
    const float *histRange = {range};

    bool uniform = true;
    bool accumulate = false;

    Mat b_hist, g_hist, r_hist;

    /// Compute the histograms:
    calcHist(&bgr_planes[0], 1, 0, Mat(), b_hist, 1, &histSize, &histRange, uniform, accumulate);
    calcHist(&bgr_planes[1], 1, 0, Mat(), g_hist, 1, &histSize, &histRange, uniform, accumulate);
    calcHist(&bgr_planes[2], 1, 0, Mat(), r_hist, 1, &histSize, &histRange, uniform, accumulate);

    // Draw the histograms for B, G and R
    int hist_w = 512;
    int hist_h = 400;
    int bin_w = cvRound((double) hist_w / histSize);

    Mat histImage(hist_h, hist_w, CV_8UC3, Scalar(0, 0, 0));

    /// Normalize the result to [ 0, histImage.rows ]
    normalize(b_hist, b_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());
    normalize(g_hist, g_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());
    normalize(r_hist, r_hist, 0, histImage.rows, NORM_MINMAX, -1, Mat());

    /// Draw for each channel
    for (int i = 1; i < histSize; i++) {
        line(histImage, Point(bin_w * (i - 1), hist_h - cvRound(b_hist.at<float>(i - 1))),
             Point(bin_w * (i), hist_h - cvRound(b_hist.at<float>(i))),
             Scalar(255, 0, 0), 2, 8, 0);
        line(histImage, Point(bin_w * (i - 1), hist_h - cvRound(g_hist.at<float>(i - 1))),
             Point(bin_w * (i), hist_h - cvRound(g_hist.at<float>(i))),
             Scalar(0, 255, 0), 2, 8, 0);
        line(histImage, Point(bin_w * (i - 1), hist_h - cvRound(r_hist.at<float>(i - 1))),
             Point(bin_w * (i), hist_h - cvRound(r_hist.at<float>(i))),
             Scalar(0, 0, 255), 2, 8, 0);
    }

    /// Display
    namedWindow("calcHist Demo", CV_WINDOW_AUTOSIZE);
    imshow("calcHist Demo", histImage);

    waitKey(0);
}

void show_hsv(const char *image_file) {
    IplImage *image, *hsv, *hue, *saturation, *value;
    image = cvLoadImage(image_file);

    hsv = cvCreateImage(cvGetSize(image), 8, 3);
    hue = cvCreateImage(cvGetSize(image), 8, 1);
    saturation = cvCreateImage(cvGetSize(image), 8, 1);
    value = cvCreateImage(cvGetSize(image), 8, 1);

    cvNamedWindow("image", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("hsv", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("hue", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("saturation", CV_WINDOW_AUTOSIZE);
    cvNamedWindow("value", CV_WINDOW_AUTOSIZE);

    cvCvtColor(image, hsv, CV_BGR2HSV);

    cvShowImage("image", image);
    cvShowImage("hsv", hsv);

    cvSplit(hsv, hue, 0, 0, 0);
    cvSplit(hsv, 0, saturation, 0, 0);
    cvSplit(hsv, 0, 0, value, 0);

    cvShowImage("hue", hue);
    cvShowImage("saturation", saturation);
    cvShowImage("value", value);
    cvWaitKey(0);
}

void show_sobel(const char *image_file) {
    Mat grad_x, grad_y;
    Mat abs_grad_x, abs_grad_y, dst;

    Mat src = imread(image_file);
    imshow("Src", src);

    Sobel(src, grad_x, CV_16S, 1, 0, 3, 1, 1, BORDER_DEFAULT);
    convertScaleAbs(grad_x, abs_grad_x);
    imshow("X Sobel", abs_grad_x);

    Sobel(src, grad_y, CV_16S, 0, 1, 3, 1, 1, BORDER_DEFAULT);
    convertScaleAbs(grad_y, abs_grad_y);
    imshow("Y Sobel", abs_grad_y);

    addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, dst);
    imshow("All Sobel", dst);

    waitKey(0);
}

Mat addSaltNoise(const Mat srcImage, int n) {
    Mat dstImage = srcImage.clone();
    for (int k = 0; k < n; k++) {
        int i = rand() % dstImage.rows;
        int j = rand() % dstImage.cols;
        if (dstImage.channels() == 1) {
            dstImage.at<uchar>(i, j) = 255;
        } else {
            dstImage.at<Vec3b>(i, j)[0] = 255;
            dstImage.at<Vec3b>(i, j)[1] = 255;
            dstImage.at<Vec3b>(i, j)[2] = 255;
        }
    }
    for (int k = 0; k < n; k++) {
        int i = rand() % dstImage.rows;
        int j = rand() % dstImage.cols;
        if (dstImage.channels() == 1) {
            dstImage.at<uchar>(i, j) = 0;
        } else {
            dstImage.at<Vec3b>(i, j)[0] = 0;
            dstImage.at<Vec3b>(i, j)[1] = 0;
            dstImage.at<Vec3b>(i, j)[2] = 0;
        }
    }
    return dstImage;
}

static double _generateGaussianNoise(double mu, double sigma) {
    const double epsilon = numeric_limits<double>::min();
    static double z0, z1;
    static bool flag = false;
    flag = !flag;
    if (!flag)
        return z1 * sigma + mu;
    double u1, u2;
    do {
        u1 = rand() * (1.0 / RAND_MAX);
        u2 = rand() * (1.0 / RAND_MAX);
    } while (u1 <= epsilon);
    z0 = sqrt(-2.0 * log(u1)) * cos(2 * CV_PI * u2);
    z1 = sqrt(-2.0 * log(u1)) * sin(2 * CV_PI * u2);
    return z0 * sigma + mu;
}

Mat addGaussianNoise(Mat &srcImag, double sigma) {
    Mat dstImage = srcImag.clone();
    int channels = dstImage.channels();
    int rowsNumber = dstImage.rows;
    int colsNumber = dstImage.cols * channels;
    if (dstImage.isContinuous()) {
        colsNumber *= rowsNumber;
        rowsNumber = 1;
    }
    for (int i = 0; i < rowsNumber; i++) {
        for (int j = 0; j < colsNumber; j++) {
            int val = static_cast<int>(dstImage.ptr<uchar>(i)[j] +
                                       _generateGaussianNoise(2, sigma) * 32);
            if (val < 0)
                val = 0;
            if (val > 255)
                val = 255;
            dstImage.ptr<uchar>(i)[j] = (uchar) val;
        }
    }
    return dstImage;
}

int main() {
    return 0;
}