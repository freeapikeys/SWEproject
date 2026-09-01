#include "kde.h"
#include <math.h>
#include <stdlib.h>
#include <opencv2/imgproc/imgproc_c.h>

IplImage* compute_heatmap(int width, int height, int npoints, int xs[], int ys[], double sigma, double scale){
    int sw = (int)(width * scale);
    int sh = (int)(height * scale);
    if(sw<1) sw=1; if(sh<1) sh=1;
    IplImage* heat = cvCreateImage(cvSize(sw,sh), IPL_DEPTH_32F, 1);
    cvZero(heat);
    double two_sigma_sq = 2.0*sigma*sigma;
    for(int p=0;p<npoints;p++){
        double cx = xs[p]*scale;
        double cy = ys[p]*scale;
        int minx = (int)fmax(0, floor(cx - 3*sigma));
        int maxx = (int)fmin(sw-1, ceil(cx + 3*sigma));
        int miny = (int)fmax(0, floor(cy - 3*sigma));
        int maxy = (int)fmin(sh-1, ceil(cy + 3*sigma));
        for(int y=miny;y<=maxy;y++){
            for(int x=minx;x<=maxx;x++){
                double dx = x - cx;
                double dy = y - cy;
                double v = exp(-(dx*dx + dy*dy)/two_sigma_sq);
                float* ptr = (float*)(heat->imageData + heat->widthStep*y);
                ptr[x] += (float)v;
            }
        }
    }
    double minv, maxv;
    cvMinMaxLoc(heat, &minv, &maxv, NULL, NULL, NULL);
    IplImage* heat8 = cvCreateImage(cvSize(sw,sh), IPL_DEPTH_8U, 1);
    if(maxv < 1e-6) cvZero(heat8);
    else{
        for(int y=0;y<sh;y++){
            float* sptr = (float*)(heat->imageData + heat->widthStep*y);
            unsigned char* dptr = (unsigned char*)(heat8->imageData + heat8->widthStep*y);
            for(int x=0;x<sw;x++){
                double v = sptr[x]/maxv * 255.0;
                if(v>255) v=255;
                dptr[x] = (unsigned char)v;
            }
        }
    }
    cvReleaseImage(&heat);
    IplImage* heatColor = cvCreateImage(cvSize(sw,sh), IPL_DEPTH_8U, 3);
    for(int y=0;y<sh;y++){
        unsigned char* sptr = (unsigned char*)(heat8->imageData + heat8->widthStep*y);
        unsigned char* dptr = (unsigned char*)(heatColor->imageData + heatColor->widthStep*y);
        for(int x=0;x<sw;x++){
            unsigned char v = sptr[x];
            int r = v>128 ? (int)(255*(v-128)/127.0) : 0;
            int b = v<128 ? (int)(255*(128-v)/128.0) : 0;
            int g = 255 - abs(v-128)*2;
            if(g<0) g=0;
            dptr[3*x+0] = b;
            dptr[3*x+1] = g;
            dptr[3*x+2] = r;
        }
    }
    cvReleaseImage(&heat8);
    return heatColor;
}
