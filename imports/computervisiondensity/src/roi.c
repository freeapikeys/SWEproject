#include "roi.h"
#include <stdlib.h>
// ray casting algorithm
int point_in_poly(int x, int y, const Polygon* poly){
    int i,j,c=0;
    for(i=0,j=poly->n-1;i<poly->n;j=i++){
        int xi=poly->xs[i], yi=poly->ys[i];
        int xj=poly->xs[j], yj=poly->ys[j];
        if ( ((yi>y) != (yj>y)) &&
             (x < (double)(xj-xi) * (y-yi) / (yj-yi + 1e-12) + xi) )
            c = !c;
    }
    return c;
}
void polygon_free(Polygon* poly){
    if(!poly) return;
    free(poly->xs);
    free(poly->ys);
    poly->n = 0;
}
