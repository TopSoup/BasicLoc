#include <AEESTDLIB.H>

#include "transform.h"

extern double e_asin(double x);

// 1 - cos(x) == 2 sin^2(x/2)
double oneMinusCos(double x)
{
	double s = FSIN(FDIV(x,2));
	return FMUL(FMUL(s,s), 2);
}


double calc_distance(double latA, double lngA, double latB, double lngB) {
	double earthR = 6371000;
	double tmp = 0;
	double test = 0;

	latA = FMUL(latA, FDIV(M_PI,180));
	latB = FMUL(latB, FDIV(M_PI,180));
	lngA = FMUL(lngA, FDIV(M_PI,180));
	lngB = FMUL(lngB, FDIV(M_PI,180));

	tmp = FSQRT(FADD(oneMinusCos(FSUB(latA, latB)), FMUL(FMUL(FCOS(latA), FCOS(latB)), oneMinusCos(FSUB(lngA, lngB)))));

	test = FMUL(FMUL(2,earthR), e_asin(FDIV(tmp, M_SQRT2)));
	return test;
}

double calc_azimuth(double latA, double lngA, double latB, double lngB) {
	//int	latFlag = 0, lngFlag = 0;
	double th = 0;
	double offset = 0;
	double latO = 0, lngO = 0;
	double disAB = 0, disBO = 0;
	double dLatAB = 0, dLngAB = 0;
#if TEST
	char szLat[64], szLon[64];
	AECHAR buf[32];
#endif
	//经纬度变化:
	//东经向东增大,西经向西增大; 南纬向南增大, 北纬向北增大

	//GPS坐标确定时区: 
	//latFlag = (latA > 0) ? 1: 0;	//>0东经 <0西经
	//lngFlag = (lngA > 0) ? 1: 0;	//>0北纬 <0南纬

	//这里只考虑东经,北纬情况, 以便确定罗盘的水平方位角: 
	//1-(>0, >0)-(0~90)		//th+0
	//2-(>0, <0)-(90~180)	//th+90
	//3-(<0, <0)-(180~270)	//th+180
	//4-(<0, >0)-(270~360)	//th+270

	dLatAB = FSUB(latB, latA);	//纵向变化
	dLngAB = FSUB(lngB, lngA);	//横向变化

#if TEST
	MEMSET(szLat,0,sizeof(szLat));
	FLOATTOWSTR(dLatAB, buf, 32);
	WSTRTOSTR(buf, szLat, 64);

	MEMSET(szLon,0,sizeof(szLon));
	FLOATTOWSTR(dLngAB, buf, 32);
	WSTRTOSTR(buf, szLon, 64);
	DBGPRINTF("dLatAB:%s dLngAB:%s", szLat, szLon);
#endif

	if (FCMP_G(FABS(FMUL(dLatAB, dLngAB)), 0))
	{
		if (FCMP_G(dLatAB, 0) && FCMP_G(dLngAB, 0))
		{
			latO = latB;
			lngO = lngA;
			offset = 0;
		}
		else if (FCMP_L(dLatAB, 0) && FCMP_G(dLngAB, 0))
		{
			latO = latA;
			lngO = lngB;
			offset = 90;
		}
		else if (FCMP_L(dLatAB, 0) && FCMP_L(dLngAB, 0))
		{
			latO = latB;
			lngO = lngA;
			offset = 180;
		}
		else if (FCMP_G(dLatAB, 0) && FCMP_L(dLngAB, 0))
		{
			latO = latA;
			lngO = lngB;
			offset = 270;
		}

		//calculate distance between A and B
		disAB = calc_distance(latA, lngA, latB, lngB);

		//calculate distance between B and O
		disBO = calc_distance(latO, lngO, latB, lngB);

		th = e_asin(FDIV(disBO, disAB));

		th = FADD(FMUL(FDIV(th, M_PI), 180), offset);
	}
	else
	{
		if (FCMP_LE(FABS(dLatAB), 0.000001) && FCMP_LE(FABS(dLngAB), 0.000001))
		{
			th = 0;
		}
		
		else if (FCMP_LE(FABS(dLatAB), 0.000001))
		{
			if (FCMP_G(dLngAB, 0))
			{
				th = 90;
			}
			else
			{
				th = 270;
			}
		}
		else if (FCMP_LE(FABS(dLngAB), 0.000001))
		{
			if (FCMP_G(dLatAB, 0))
			{
				th = 0;
			}
			else
			{
				th = 180;
			}
		}
	}
	
	return th;
}




