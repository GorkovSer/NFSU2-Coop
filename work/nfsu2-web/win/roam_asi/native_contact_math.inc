static float nw_len2(const float *v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
static float nw_dist2(const float *a,const float *b){float v[3];for(int k=0;k<3;k++)v[k]=a[k]-b[k];return nw_len2(v);}
