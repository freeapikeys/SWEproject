# utils.h
#ifndef UTILS_H
#define UTILS_H
#include <opencv2/core/core_c.h>
#include <opencv2/highgui/highgui_c.h>
#include <time.h>

double now_seconds();
void draw_text(IplImage* img, const char* text, int x, int y, CvScalar color);
#endif
