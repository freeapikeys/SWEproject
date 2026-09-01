#include "detector_fake.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/video/background_segm_c.h>
#include <stdlib.h>
#include <stdio.h>

int fake_detect(IplImage* frame, int *count, int xs[], int ys[], int ws[], int hs[]){
    static CvBGCodebookModel* bg = NULL;
    static int initialized = 0;
    if(!initialized){
        // fallback: simple running background model through cvCreateBackgroundSubtractorMOG2 is C++ => use simple frame-diff approach
        initialized = 1;
    }
    // Convert to gray and do simple frame diff with a tiny blur (frame differencing needs previous frame; we'll do naive thresholding)
    IplImage* gray = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 1);
    cvCvtColor(frame, gray, CV_BGR2GRAY);
    IplImage* blur = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 1);
    cvSmooth(gray, blur, CV_GAUSSIAN, 9,9,0,0);
    static IplImage* prev = NULL;
    if(!prev) prev = cvCloneImage(blur);
    IplImage* diff = cvCreateImage(cvGetSize(frame), IPL_DEPTH_8U, 1);
    cvAbsDiff(blur, prev, diff);
    cvThreshold(diff, diff, 25, 255, CV_THRESH_BINARY);
    // morphological clean
    cvDilate(diff, diff, NULL, 2);
    cvErode(diff, diff, NULL, 2);
    // find contours
    CvMemStorage* storage = cvCreateMemStorage(0);
    CvSeq* contours = NULL;
    cvFindContours(diff, storage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
    int n=0;
    for(CvSeq* c=contours;c!=NULL;c=c->h_next){
        CvRect r = cvBoundingRect(c,0);
        if(r.width*r.height < 400) continue;
        xs[n]=r.x; ys[n]=r.y; ws[n]=r.width; hs[n]=r.height;
        n++;
        if(n>=100) break;
    }
    *count = n;
    cvReleaseMemStorage(&storage);
    cvReleaseImage(&diff);
    cvReleaseImage(&blur);
    cvReleaseImage(&gray);
    cvCopyImage(blur, prev);
    return 0;
}
