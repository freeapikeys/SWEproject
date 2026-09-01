#ifndef KDE_H
#define KDE_H
#include <opencv2/core/core_c.h>
IplImage* compute_heatmap(int width, int height, int npoints, int xs[], int ys[], double sigma, double scale);
#endif
