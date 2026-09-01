#ifndef DETECTOR_ONNX_H
#define DETECTOR_ONNX_H
#include <opencv2/core/core_c.h>

int onnx_init(const char* model_path);
int onnx_detect(IplImage* frame, int *count, int xs[], int ys[], int ws[], int hs[]);
void onnx_release();

#endif
