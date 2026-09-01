#include "detector_onnx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <onnxruntime_c_api.h>

static OrtEnv* ort_env = NULL;
static OrtSession* session = NULL;
static OrtSessionOptions* session_opts = NULL;
static OrtAllocator* allocator = NULL;
static const OrtApi* g_ort = NULL;
static int input_w = 640;
static int input_h = 640;
static float conf_thresh = 0.25f;
static float iou_thresh = 0.45f;

int onnx_init(const char* model_path){
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "crowd", &ort_env) != ORT_OK) return -1;
    if(g_ort->CreateSessionOptions(&session_opts) != ORT_OK) return -1;
    g_ort->SetIntraOpNumThreads(session_opts, 1);
    if(g_ort->CreateSession(ort_env, model_path, session_opts, &session) != ORT_OK) return -1;
    g_ort->GetAllocatorWithDefaultOptions(&allocator);
    return 0;
}

static void nms_keep(int ndets, float boxes[][4], float scores[], int keep[], int* keep_n){
    int picked[1024] = {0};
    int out_n = 0;
    for(int i=0;i<ndets;i++){
        if(picked[i]) continue;
        keep[out_n++] = i;
        float ax1 = boxes[i][0], ay1 = boxes[i][1], ax2 = boxes[i][2], ay2 = boxes[i][3];
        for(int j=i+1;j<ndets;j++){
            if(picked[j]) continue;
            float bx1 = boxes[j][0], by1 = boxes[j][1], bx2 = boxes[j][2], by2 = boxes[j][3];
            float ix1 = fmax(ax1,bx1), iy1 = fmax(ay1,by1);
            float ix2 = fmin(ax2,bx2), iy2 = fmin(ay2,by2);
            float iw = ix2 - ix1, ih = iy2 - iy1;
            if(iw>0 && ih>0){
                float inter = iw*ih;
                float aarea = (ax2-ax1)*(ay2-ay1);
                float barea = (bx2-bx1)*(by2-by1);
                float iou = inter / (aarea + barea - inter + 1e-6f);
                if(iou > iou_thresh) picked[j]=1;
            }
        }
    }
    *keep_n = out_n;
}

int onnx_detect(IplImage* frame, int *count, int xs[], int ys[], int ws[], int hs[]){
    if(!session) return -1;
    IplImage* resized = cvCreateImage(cvSize(input_w,input_h), IPL_DEPTH_8U, 3);
    cvResize(frame, resized, CV_INTER_LINEAR);
    IplImage* rgb = cvCreateImage(cvSize(input_w,input_h), IPL_DEPTH_8U, 3);
    cvCvtColor(resized, rgb, CV_BGR2RGB);
    int img_size = input_w * input_h * 3;
    float* input_data = (float*)malloc(sizeof(float)*img_size);
    int csize = input_w*input_h;
    for(int y=0;y<input_h;y++){
        for(int x=0;x<input_w;x++){
            unsigned char* p = (unsigned char*)(rgb->imageData + rgb->widthStep*y) + 3*x;
            int r = p[0], g = p[1], b = p[2];
            int idx = y*input_w + x;
            input_data[0*csize + idx] = r / 255.0f;
            input_data[1*csize + idx] = g / 255.0f;
            input_data[2*csize + idx] = b / 255.0f;
        }
    }
    OrtMemoryInfo* meminfo;
    g_ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &meminfo);
    int64_t shape[4] = {1, 3, input_h, input_w};
    OrtValue* input_tensor = NULL;
    g_ort->CreateTensorWithDataAsOrtValue(meminfo, input_data, sizeof(float)*img_size, shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensor);
    g_ort->ReleaseMemoryInfo(meminfo);

    char* input_name = NULL; char* output_name = NULL;
    OrtAllocatedStringPtr in_name_ptr; OrtAllocatedStringPtr out_name_ptr;
    size_t num_input_nodes = 0, num_output_nodes = 0;
    g_ort->SessionGetInputCount(session, &num_input_nodes);
    g_ort->SessionGetOutputCount(session, &num_output_nodes);
    g_ort->SessionGetInputName(session, 0, allocator, &input_name);
    g_ort->SessionGetOutputName(session, 0, allocator, &output_name);

    const char* input_names[] = {input_name};
    const char* output_names[] = {output_name};
    OrtValue* output_tensor = NULL;
    g_ort->Run(session, NULL, input_names, (const OrtValue* const*)&input_tensor, 1, output_names, 1, &output_tensor);

    float* outptr = NULL;
    OrtTensorTypeAndShapeInfo* info = NULL;
    g_ort->GetTensorTypeAndShape(output_tensor, &info);
    size_t total_len = 1; size_t dim_count = 0; g_ort->GetDimensionsCount(info, &dim_count);
    int64_t* dims = (int64_t*)malloc(sizeof(int64_t)*dim_count);
    g_ort->GetDimensions(info, dims, dim_count);
    for(size_t i=0;i<dim_count;i++) total_len *= dims[i];
    g_ort->GetTensorMutableData(output_tensor, (void**)&outptr);

    int N = (int)dims[1];
    int D = (int)dims[2];
    float boxes[4096][4]; float scores[4096]; int detcount=0;
    for(int i=0;i<N;i++){
        float bx = outptr[i*D + 0];
        float by = outptr[i*D + 1];
        float bw = outptr[i*D + 2];
        float bh = outptr[i*D + 3];
        float obj_conf = outptr[i*D + 4];
        float maxc = 0.0f; int cls = -1;
        for(int c=0;c<D-5;c++){
            float v = outptr[i*D + 5 + c];
            if(v>maxc){ maxc=v; cls=c; }
        }
        float final_conf = obj_conf * maxc;
        if(cls==0 && final_conf > conf_thresh){
            float xcenter = bx * frame->width;
            float ycenter = by * frame->height;
            float wbox = bw * frame->width;
            float hbox = bh * frame->height;
            float x1 = xcenter - wbox/2.0f;
            float y1 = ycenter - hbox/2.0f;
            float x2 = xcenter + wbox/2.0f;
            float y2 = ycenter + hbox/2.0f;
            boxes[detcount][0] = x1; boxes[detcount][1] = y1; boxes[detcount][2] = x2; boxes[detcount][3] = y2;
            scores[detcount] = final_conf;
            detcount++;
            if(detcount>=4096) break;
        }
    }

    int keep_idx[4096]; int keep_n=0;
    if(detcount>0) nms_keep(detcount, boxes, scores, keep_idx, &keep_n);

    int out_n = 0;
    for(int k=0;k<keep_n;k++){
        int i = keep_idx[k];
        float x1 = boxes[i][0], y1=boxes[i][1], x2=boxes[i][2], y2=boxes[i][3];
        int ix = (int)fmax(0, floor(x1));
        int iy = (int)fmax(0, floor(y1));
        int iw = (int)fmin(frame->width-1, ceil(x2)) - ix;
        int ih = (int)fmin(frame->height-1, ceil(y2)) - iy;
        if(iw<=0 || ih<=0) continue;
        xs[out_n]=ix; ys[out_n]=iy; ws[out_n]=iw; hs[out_n]=ih; out_n++;
        if(out_n>=512) break;
    }
    *count = out_n;

    g_ort->ReleaseTensorTypeAndShapeInfo(info);
    g_ort->ReleaseValue(output_tensor);
    g_ort->ReleaseValue(input_tensor);
    free(dims);
    free(input_data);
    cvReleaseImage(&resized);
    cvReleaseImage(&rgb);
    g_ort->AllocatorFree(allocator, input_name);
    g_ort->AllocatorFree(allocator, output_name);
    return 0;
}

void onnx_release(){
    if(session) g_ort->ReleaseSession(session);
    if(session_opts) g_ort->ReleaseSessionOptions(session_opts);
    if(ort_env) g_ort->ReleaseEnv(ort_env);
}
