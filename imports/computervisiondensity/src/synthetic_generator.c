#include <opencv2/opencv.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

int main(int argc,char** argv){
    if(argc<4){
        printf("Usage: synthetic_generator out.mp4 width height [fps]\n");
        return 1;
    }
    const char* out = argv[1];
    int w = atoi(argv[2]);
    int h = atoi(argv[3]);
    int fps = (argc>4)?atoi(argv[4]):25;
    CvSize size = cvSize(w,h);
    CvVideoWriter* writer = cvCreateVideoWriter(out, CV_FOURCC('m','p','4','v'), fps, size, 1);
    if(!writer){ fprintf(stderr,"Could not create writer\n"); return 1; }
    srand(time(NULL));
    int npeople = 8;
    float x[32], y[32], vx[32], vy[32];
    for(int i=0;i<npeople;i++){
        x[i] = rand()%(w-100)+50;
        y[i] = rand()%(h-100)+50;
        vx[i] = ((rand()%100)/100.0f - 0.5f)*4.0f;
        vy[i] = ((rand()%100)/100.0f - 0.5f)*4.0f;
    }
    for(int f=0; f<900; f++){
        IplImage* img = cvCreateImage(size, IPL_DEPTH_8U, 3);
        cvZero(img);
        for(int i=0;i<npeople;i++){
            x[i]+=vx[i]; y[i]+=vy[i];
            if(x[i]<20 || x[i]>w-20) vx[i]*=-1;
            if(y[i]<20 || y[i]>h-20) vy[i]*=-1;
            CvPoint center = cvPoint((int)x[i], (int)y[i]);
            cvCircle(img, center, 12, CV_RGB(0,200,0), -1, 8, 0);
            // draw head shadow
            cvCircle(img, cvPoint((int)x[i]+3, (int)y[i]+3), 12, CV_RGB(0,100,0), -1, 8, 0);
        }
        cvWriteFrame(writer, img);
        cvReleaseImage(&img);
    }
    cvReleaseVideoWriter(&writer);
    printf("generated %s\n", out);
    return 0;
}
