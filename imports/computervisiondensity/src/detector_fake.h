#ifndef DETECTOR_FAKE_H
#define DETECTOR_FAKE_H
#include <opencv2/core/core_c.h>
int fake_detect(IplImage* frame, int *count, int xs[], int ys[], int ws[], int hs[]);
#endif
