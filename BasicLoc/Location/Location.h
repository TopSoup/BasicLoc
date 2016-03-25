#ifndef __LOCATION_H__
#define __LOCATION_H__

/*======================================================
FILE:  SP_Track.h

SERVICES: Tracking

GENERAL DESCRIPTION:
	Sample code to demonstrate services of IPosDet.

        Copyright ? 2003 QUALCOMM Incorporated.
               All Rights Reserved.
            QUALCOMM Proprietary/GTDR
=====================================================*/
#include "AEEComdef.h"
#include "AEEPosDet.h"

#define LOC_CONFIG_FILE "gpsConfig.txt"
#define LOC_CONFIG_OPT_STRING       "GPS_OPTIMIZATION_MODE = "
#define LOC_CONFIG_QOS_STRING       "GPS_QOS = "
#define LOC_CONFIG_SVR_TYPE_STRING  "GPS_SERVER_TYPE = "
#define LOC_CONFIG_SVR_IP_STRING    "GPS_SERVER_IP = "
#define LOC_CONFIG_SVR_PORT_STRING  "GPS_SERVER_PORT = "

 #define LOC_QOS_DEFAULT       127

typedef struct _LocState LocState;

typedef struct {
	double lat;       /* latitude on WGS-84 Geoid */
	double lon;       /* longitude on WGS-84 Geoid */
}Coordinate;

typedef struct {
   int    nErr;      /* SUCCESS or AEEGPS_ERR_* */
   uint32 dwFixNum;  /* fix number in this tracking session. */
   double lat;       /* latitude on WGS-84 Geoid */
   double lon;       /* longitude on WGS-84 Geoid */
   short  height;    /* Height from WGS-84 Geoid */
   double velocityHor; /* Horizontal velocity, meters/second*/
   double heading;	 /*  Current Heading degrees  */
   
   //Config Param
   AEEGPSConfig gpsConfig;

   Coordinate destPos;	/* Specifies destination */
   boolean bSetDestPos;

}PositionData;

typedef struct _GetGPSInfo {
   PositionData theInfo;
   IPosDet      *pPosDet;
   AEECallback  cbPosDet;
   uint32       dwFixNumber;
   uint32       dwFixDuration;
   uint32       dwFail;
   uint32       dwTimeout;
   uint32       wProgress;
   uint32       wIdleCount;
   boolean      bPaused;
   boolean      bAbort;
   LocState		*pts;
}GetGPSInfo;

#ifdef __cplusplus
extern "C" {
#endif

   /* Return Values : 
             SUCCESS
             EBADPARM - One or more of the Invalid arguments
             EUNSUPPORTED - Unimplemented
             ENOMEMORY - When system is out of memory.
             EFAILED - General failure.
             EALREADY - When tracking is already in progress.


      Values in PositionData::nErr :
             SUCCESS
             AEEGPS_ERR_*
             EIDLE - When a session is halted or done.
   */


   /* Creates and initializes a handle for tracking. 
   ** Invoke the CALLBACK_Cancel( pcb ) to destroy this object. */
   int Loc_Init( IShell *pIShell, IPosDet *pIPos, AEECallback *pcb, LocState **po );

   /* Starts the tracking using the object created in Loc_Init */
   int Loc_Start( LocState *pts, PositionData *pData );

   /* Stops the tracking, does not clean up the object, it can be
   ** further used with Loc_Start. Only CALLBACK_Cancel(pcb) releases
   ** the object. */
   int Loc_Stop( LocState *pts );

#ifdef __cplusplus
}
#endif

#endif