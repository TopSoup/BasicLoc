#ifndef TRANSFORM_HEADER
#define TRANSFORM_HEADER

#define M_PI 3.14159265358979323846
#define M_SQRT2 1.41421356237309504880
#define M_D2R FDIV(M_PI, 180.0)	//degrees to rad

//�����������
double calc_distance(double latA, double lngA, double latB, double lngB);

//���������ˮƽ��λ��
double calc_azimuth(double latA, double lngA, double latB, double lngB);

#endif
