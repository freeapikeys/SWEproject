#include "utils.h"
#include <stdio.h>
#include <sys/time.h>
#include <opencv2/imgproc/imgproc_c.h>

double now_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec*1e-6;
}
void draw_text(IplImage* img, const char* text, int x, int y, CvScalar color){
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5, 0, 1, CV_AA);
    cvPutText(img, text, cvPoint(x,y), &font, color);
}
