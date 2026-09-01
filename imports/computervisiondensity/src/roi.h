#ifndef ROI_H
#define ROI_H
typedef struct {
    int *xs;
    int *ys;
    int n;
} Polygon;
int point_in_poly(int x, int y, const Polygon* poly);
void polygon_free(Polygon* poly);
#endif
