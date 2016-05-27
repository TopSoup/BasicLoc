/*======================================================
FILE:  SP_Loc.c

SERVICES: Locing using IPosDet.
  Loc_Init
  Loc_Start
  Loc_Stop

GENERAL DESCRIPTION:
	Sample code to demonstrate services of IPosDet. See SP_Loc.h for the
   description of exported functions.

        Copyright ?2003 QUALCOMM Incorporated.
               All Rights Reserved.
            QUALCOMM Proprietary/GTDR
=====================================================*/
#include "AEEComdef.h"
#include "BREWVersion.h"
#include "AEEStdLib.h"
#include "AEEFile.h"
#include "location.h"

struct _LocState{
   boolean     bInNotification;     /* When the state machine is notifying the client */
   boolean     bSetForCancellation; /* Loc is meant to be cancelled. do so when it is safe. */
   boolean     bInProgress;         /* when Locing is in progress. */
/* For Dest Position */
   boolean	   bSetDestPos;

/**************************/

/* Private members to work with IPosDet */
   AEECallback cbIntervalTimer;
   AEECallback cbInfo;
   AEEGPSInfo theInfo;
   Coordinate lastCoordinate;
   
/**************************/

/* Clients response members. */
   AEECallback *pcbResp;
   PositionData *pResp;
/**************************/

/* Client passed members. */
   int nLocInterval;
   IPosDet    *pPos;
   IShell     *pShell;
/**************************/
};


/*======================================================================= 
Function: Loc_ReadGPSSettings()

Description: 
   Reads the GPS configuration settings from the configuration file.
=======================================================================*/
static uint32 Loc_ReadGPSSettings(IFile * pIFile, AEEGPSConfig *gpsConfig);

/*======================================================================= 
Function: Loc_WriteGPSSettings()

Description: 
   Write the GPS configuration settings from the configuration file.

Prototype:

   uint32 Loc_WriteGPSSettings(CLoc *pMe, IFile * pIFile);
=======================================================================*/
static uint32 Loc_WriteGPSSettings( IFile * pIFile , AEEGPSConfig *gpsConfig);

static void Loc_Notify( LocState *pts )
{
   pts->bInNotification = TRUE;
   pts->pcbResp->pfnNotify( pts->pcbResp->pNotifyData );
   pts->bInNotification = FALSE;
}

static void Loc_Cancel( AEECallback *pcb )
{
   LocState *pts = (LocState *)pcb->pCancelData;

   if( TRUE == pts->bInNotification ) {
      /* It is not safe to cleanup from a notification. Defer it. */
      pts->bSetForCancellation = TRUE;
      return;
   }

   /* Kill any ongoing process */
   CALLBACK_Cancel( &pts->cbInfo );
   CALLBACK_Cancel( &pts->cbIntervalTimer );
   
   pts->pcbResp->pfnCancel = 0;
   pts->pcbResp->pCancelData = 0;

   IPOSDET_Release( pts->pPos );
   ISHELL_Release( pts->pShell );

   FREE( pts );
}

static void Loc_cbInterval( LocState *pts )
{
   /* Cancel if it was deferred. */
   if( TRUE == pts->bSetForCancellation ) {

      Loc_Cancel( pts->pcbResp );
      return;
   }

   DBGPRINTF( "@Loc_cbInterval ");

   // Request GPSInfo
   if( TRUE == pts->bInProgress && SUCCESS != IPOSDET_GetGPSInfo( pts->pPos, 
	   AEEGPS_GETINFO_LOCATION | AEEGPS_GETINFO_VELOCITY | AEEGPS_GETINFO_ALTITUDE | AEEGPS_GETINFO_VERSION_1, 
	   AEEGPS_ACCURACY_LEVEL1, &pts->theInfo, &pts->cbInfo ) ) {

	  DBGPRINTF( "IPOSDET_GetGPSInfo Failed!");
	  
      /* Report a failure and bailout */
      pts->pResp->nErr = AEEGPS_ERR_GENERAL_FAILURE;

      Loc_Notify( pts );

      Loc_Stop( pts );

   }
}

static void Loc_Network( LocState *pts )
{
   AEEGPSConfig config;

   DBGPRINTF( "Loc_Network" );
   
   (void) IPOSDET_GetGPSConfig( pts->pPos, &config );

   config.mode = AEEGPS_MODE_TRACK_NETWORK;

   (void) IPOSDET_SetGPSConfig( pts->pPos, &config );
}

static void Loc_StandAlone( LocState *pts )
{
   AEEGPSConfig config;

   DBGPRINTF( "Loc_StandAlone" );

   (void) IPOSDET_GetGPSConfig( pts->pPos, &config );

   config.mode = AEEGPS_MODE_TRACK_STANDALONE;

   (void) IPOSDET_SetGPSConfig( pts->pPos, &config );
}

static void Loc_cbInfo( LocState *pts ) {
	
	if( pts->theInfo.status == AEEGPS_ERR_NO_ERR 
		|| (pts->theInfo.status == AEEGPS_ERR_INFO_UNAVAIL && pts->theInfo.fValid) ) {
		
#if MIN_BREW_VERSION(2,1)
		pts->pResp->lat = WGS84_TO_DEGREES( pts->theInfo.dwLat );
#ifdef AEE_SIMULATOR
		//FOR TEST
		pts->pResp->lon = -WGS84_TO_DEGREES( pts->theInfo.dwLon );
#else
		pts->pResp->lon = WGS84_TO_DEGREES( pts->theInfo.dwLon );
#endif
#else
		double    wgsFactor;
		wgsFactor = FASSIGN_STR("186413.5111");
		pts->pResp->lat = FASSIGN_INT(pts->theInfo.dwLat);
		pts->pResp->lat = FDIV(pts->pResp->lat, wgsFactor);
		
		pts->pResp->lon = FASSIGN_INT(pts->theInfo.dwLon);
		pts->pResp->lon = FDIV(pts->pResp->lon, wgsFactor);
#endif /* MIN_BREW_VERSION 2.1 */
		
		pts->pResp->height = pts->theInfo.wAltitude - 500;
		pts->pResp->velocityHor = FMUL( pts->theInfo.wVelocityHor,0.25);
		
		//当前夹角
		if (FCMP_G(FABS(pts->lastCoordinate.lat), 0))
		{
			pts->pResp->heading = Loc_Calc_Azimuth(pts->lastCoordinate.lat, pts->lastCoordinate.lon, pts->pResp->lat, pts->pResp->lon);
		}
		else
		{
			pts->pResp->heading = 0;
		}

		//For Test Hack
#ifdef AEE_SIMULATOR
		pts->pResp->lat = 38.0422378880;
		pts->pResp->lon = 114.4925141047;
#endif
		if (pts->pResp->bSetDestPos)
		{
			//计算距离和方位角
			pts->pResp->distance = Loc_Calc_Distance(pts->pResp->lat, pts->pResp->lon, pts->pResp->destPos.lat, pts->pResp->destPos.lon);
			pts->pResp->destHeading = Loc_Calc_Azimuth(pts->pResp->lat, pts->pResp->lon, pts->pResp->destPos.lat, pts->pResp->destPos.lon);
		}
		
		//记录历史定位信息
		pts->lastCoordinate.lat = pts->pResp->lat;
		pts->lastCoordinate.lon = pts->pResp->lon;
		
		pts->pResp->dwFixNum++;
		
		pts->pResp->nErr = SUCCESS;
		
		Loc_Notify( pts );
		
		if( FALSE == pts->bSetForCancellation ) {
			
			ISHELL_SetTimerEx( pts->pShell, pts->nLocInterval * 1000, &pts->cbIntervalTimer );
		}
		else {
			
			Loc_Stop( pts );
		}
	}
}


/*======================================================================= 
Function: Loc_Init()
=======================================================================*/
int Loc_Init( IShell *pIShell, IPosDet *pIPos, AEECallback *pcb, LocState **po ) {
   int nErr = SUCCESS;
   LocState *pts = NULL;

   if( !pIShell || !pIPos || !pcb || !po ) {

      nErr = EBADPARM;

   }
   else if( NULL == (pts = MALLOC( sizeof(LocState) )) ){

      nErr = ENOMEMORY;

   }
   else {

      ZEROAT( pts );

      pts->pShell = pIShell;
      ISHELL_AddRef( pIShell );

      pts->pPos = pIPos;
      IPOSDET_AddRef( pIPos );

      /* Install the notification cb */
      CALLBACK_Cancel( pcb );
      pts->pcbResp = pcb;
      pts->pcbResp->pfnCancel   = Loc_Cancel;
      pts->pcbResp->pCancelData = pts;

      CALLBACK_Init( &pts->cbIntervalTimer, Loc_cbInterval, pts );
      CALLBACK_Init( &pts->cbInfo, Loc_cbInfo, pts );
   }

   *po = pts;
   return nErr;

}

/*======================================================================= 
Function: Loc_Stop()
=======================================================================*/
int Loc_Stop( LocState *pts )
{
   if( !pts ) {
      return EBADPARM;
   }

   pts->bInProgress = FALSE;

   /* Kill any ongoing process */
   CALLBACK_Cancel( &pts->cbInfo );
   CALLBACK_Cancel( &pts->cbIntervalTimer );

   /* Report that Locing is halted */
   pts->pResp->nErr = EIDLE;
   
   //Loc_Notify( pts );

   if( TRUE == pts->bSetForCancellation ) {

      Loc_Cancel( pts->pcbResp );
   }
   
   return SUCCESS;
}

/*======================================================================= 
Function: Loc_Start()
=======================================================================*/
int Loc_Start( LocState *pts, PositionData *pData )
{
   int nErr = SUCCESS;

   if( !pts || !pData ) {

      nErr = EBADPARM;
   }
   else if( TRUE == pts->bInProgress ) {

      nErr = EALREADY;
   }
   else {

      AEEGPSConfig config;

      pData->dwFixNum     = 0;

      pts->pResp          = pData;
      pts->nLocInterval = pData->gpsConfig.nInterval;

      IPOSDET_GetGPSConfig( pts->pPos, &config );

      /* Configure the IPosDet Instance */
      config.mode = pData->gpsConfig.mode;
      config.nFixes = pData->gpsConfig.nFixes;
      config.nInterval = pData->gpsConfig.nInterval;

      // ADDING SUPPORT FOR USER-DEFINED PDE IP ADDRESS AND QOS
      config.server = pData->gpsConfig.server;
      config.optim = pData->gpsConfig.optim;
      config.qos = pData->gpsConfig.qos;

      nErr = IPOSDET_SetGPSConfig( pts->pPos, &config );

      if( nErr == EUNSUPPORTED && config.mode == AEEGPS_MODE_TRACK_NETWORK ) {

         /* As AEEGPS_MODE_TRACK_NETWORK is unsupported on certain devices.
         ** and we tried to Loc as AEEGPS_MODE_TRACK_STANDALONE. */

         config.mode = AEEGPS_MODE_TRACK_STANDALONE;

         nErr = IPOSDET_SetGPSConfig( pts->pPos, &config );
      }

      if( nErr == SUCCESS ) {

         pts->bInProgress = TRUE;

         Loc_cbInterval( pts );
      }
   }

   return nErr;
}

/* Calculate the distance between A and B */
double Loc_Calc_Distance( double latA, double lngA, double latB, double lngB )
{
	return calc_distance(latA, lngA, latB, lngB);
}

/* Calculate the Azimuth between A and B */
double Loc_Calc_Azimuth( double latA, double lngA, double latB, double lngB )
{
	return calc_azimuth(latA, lngA, latB, lngB);
}

/*======================================================================= 
Function: Loc_InitGPSSettings()

Description: 
   Initializes the GPS configuration to the default values.
=======================================================================*/
uint32 Loc_InitGPSSettings(IShell *pIShell, AEEGPSConfig *gpsConfig)
{
   IFileMgr   *pIFileMgr = NULL;
   IFile      *pIConfigFile = NULL;
   uint32      nResult = 0;
   
   if (pIShell == NULL || gpsConfig == NULL)
      return EFAILED;

   // Create the instance of IFileMgr
   nResult = ISHELL_CreateInstance( pIShell, AEECLSID_FILEMGR, (void**)&pIFileMgr );
   if ( SUCCESS != nResult ) {
      return nResult;
   }

   // If the config file exists, open it and read the settings.  Otherwise, we need to
   // create a new config file.
   nResult = IFILEMGR_Test( pIFileMgr, LOC_CONFIG_FILE );
   if ( SUCCESS == nResult ) {
      pIConfigFile = IFILEMGR_OpenFile( pIFileMgr, LOC_CONFIG_FILE, _OFM_READ );
      if ( !pIConfigFile ) {
         nResult = EFAILED;
      }
      else {
         nResult = Loc_ReadGPSSettings( pIConfigFile, gpsConfig );
      }
   }
   else {
      pIConfigFile = IFILEMGR_OpenFile( pIFileMgr, LOC_CONFIG_FILE, _OFM_CREATE );
      if ( !pIConfigFile ) {
         nResult = EFAILED;
      }
      else {
         // Setup the default GPS settings
         gpsConfig->optim  = AEEGPS_OPT_DEFAULT;
         gpsConfig->qos    = LOC_QOS_DEFAULT;
         gpsConfig->server.svrType = AEEGPS_SERVER_DEFAULT;
         nResult = Loc_WriteGPSSettings( pIConfigFile , gpsConfig);
      }
   }

   // Free the IFileMgr and IFile instances
   IFILE_Release( pIConfigFile );
   IFILEMGR_Release( pIFileMgr );

   return nResult;
}



/*======================================================================= 
Function: DistToSemi()

Description: 
   Utility function that determines index of the first semicolon in the
   input string.
=======================================================================*/
static int DistToSemi(const char * pszStr)
{
   int nCount = 0;

   if ( !pszStr ) { 
      return -1;
   }

   while ( *pszStr != 0 ) {
      if ( *pszStr == ';' ) {
         return nCount;
      }
      else {
         nCount++;
         pszStr++;
      }
   }

   return -1;
}



/*======================================================================= 
Function: Loc_ReadGPSSettings()

Description: 
   Reads the GPS configuration settings from the configuration file.
=======================================================================*/
static uint32 Loc_ReadGPSSettings(IFile * pIFile, AEEGPSConfig *gpsConfig)
{
   char    *pszBuf = NULL;
   char    *pszTok = NULL;
   char    *pszSvr = NULL;
   char    *pszDelimiter = ";";
   int32   nResult = 0;
   FileInfo fiInfo;

   if (pIFile == NULL || gpsConfig == NULL)
      return EFAILED;

   if ( SUCCESS != IFILE_GetInfo( pIFile, &fiInfo ) ) {
      return EFAILED;
   }

   if ( fiInfo.dwSize == 0 ) {
      return EFAILED;
   }

   // Allocate enough memory to read the full text into memory
   pszBuf = MALLOC( fiInfo.dwSize );

   nResult = IFILE_Read( pIFile, pszBuf, fiInfo.dwSize );
   if ( (uint32)nResult < fiInfo.dwSize ) {
      FREE( pszBuf );
      return EFAILED;
   }

   // Check for an optimization mode setting in the file:
   pszTok = STRSTR( pszBuf, LOC_CONFIG_OPT_STRING );
   if ( pszTok ) {
      pszTok = pszTok + STRLEN( LOC_CONFIG_OPT_STRING );
      gpsConfig->optim = (AEEGPSOpt)STRTOUL( pszTok, &pszDelimiter, 10 );
   }

   // Check for a QoS setting in the file:
   pszTok = STRSTR( pszBuf, LOC_CONFIG_QOS_STRING );
   if ( pszTok ) {
      pszTok = pszTok + STRLEN( LOC_CONFIG_QOS_STRING );
      gpsConfig->qos = (AEEGPSQos)STRTOUL( pszTok, &pszDelimiter, 10 );
   }

   // Check for a server type setting in the file:
   pszTok = STRSTR( pszBuf, LOC_CONFIG_SVR_TYPE_STRING );
   if ( pszTok ) {
      pszTok = pszTok + STRLEN( LOC_CONFIG_SVR_TYPE_STRING );
      gpsConfig->server.svrType = STRTOUL( pszTok, &pszDelimiter, 10 );

      // If the server type is IP, we need to find the ip address and the port number
      if ( AEEGPS_SERVER_IP == gpsConfig->server.svrType ) {
         pszTok = STRSTR( pszBuf, LOC_CONFIG_SVR_IP_STRING );
         if ( pszTok ) {
            pszTok = pszTok + STRLEN( LOC_CONFIG_SVR_IP_STRING );
            nResult = DistToSemi( pszTok );
            pszSvr = MALLOC( nResult+1 );
            STRNCPY( pszSvr, pszTok, nResult );
            *(pszSvr+nResult) = 0;  // Need to manually NULL-terminate the string
            if ( !INET_ATON( pszSvr, &gpsConfig->server.svr.ipsvr.addr ) ) {
               FREE( pszBuf );
               FREE( pszSvr );
               return EFAILED;
            }
            FREE( pszSvr );
         }
         pszTok = STRSTR( pszBuf, LOC_CONFIG_SVR_PORT_STRING );
         if ( pszTok ) {
            pszTok = pszTok + STRLEN( LOC_CONFIG_SVR_PORT_STRING );
            gpsConfig->server.svr.ipsvr.port = AEE_htons((INPort)STRTOUL( pszTok, &pszDelimiter, 10 ));
         }
      }
   }

   FREE( pszBuf );
   
   return SUCCESS;
}


/*======================================================================= 
Function: Loc_WriteGPSSettings()

Description: 
   Write the GPS configuration settings from the configuration file.

Prototype:

   uint32 Loc_WriteGPSSettings(CLoc *pMe, IFile * pIFile);
=======================================================================*/
static uint32 Loc_WriteGPSSettings( IFile * pIFile , AEEGPSConfig *gpsConfig)
{
   char    *pszBuf;
   int32    nResult;

   if (pIFile == NULL || gpsConfig == NULL)
      return EFAILED;

   pszBuf = MALLOC( 1024 );

   // Truncate the file, in case it already contains data
   IFILE_Truncate( pIFile, 0 );

   // Write out the optimization setting:
   SPRINTF( pszBuf, LOC_CONFIG_OPT_STRING"%d;\r\n", gpsConfig->optim );
   nResult = IFILE_Write( pIFile, pszBuf, STRLEN( pszBuf ) );
   if ( 0 == nResult ) {
      FREE(pszBuf);
      return EFAILED;
   }
   
   // Write out the QoS setting:
   SPRINTF( pszBuf, LOC_CONFIG_QOS_STRING"%d;\r\n", gpsConfig->qos );
   nResult = IFILE_Write( pIFile, pszBuf, STRLEN( pszBuf ) );
   if ( 0 == nResult ) {
      FREE(pszBuf);
      return EFAILED;
   }
   
   // Write out the server type setting:
   SPRINTF( pszBuf, LOC_CONFIG_SVR_TYPE_STRING"%d;\r\n", gpsConfig->server.svrType );
   nResult = IFILE_Write( pIFile, pszBuf, STRLEN( pszBuf ) );
   if ( 0 == nResult ) {
      FREE(pszBuf);
      return EFAILED;
   }
   
   if ( AEEGPS_SERVER_IP == gpsConfig->server.svrType ) {
      // Write out the IP address setting:
      INET_NTOA( gpsConfig->server.svr.ipsvr.addr, pszBuf, 50 );
      nResult = IFILE_Write( pIFile, LOC_CONFIG_SVR_IP_STRING, STRLEN( LOC_CONFIG_SVR_IP_STRING ) );
      if ( 0 == nResult ) {
         FREE(pszBuf);
         return EFAILED;
      }
      nResult = IFILE_Write( pIFile, pszBuf, STRLEN( pszBuf ) );
      if ( 0 == nResult ) {
         FREE(pszBuf);
         return EFAILED;
      }
      nResult = IFILE_Write( pIFile, ";\r\n", STRLEN( ";\r\n" ) );
      if ( 0 == nResult ) {
         FREE(pszBuf);
         return EFAILED;
      }

      // Write out the port setting:
      SPRINTF( pszBuf, LOC_CONFIG_SVR_PORT_STRING"%d;\r\n", AEE_ntohs(gpsConfig->server.svr.ipsvr.port) );
      nResult = IFILE_Write( pIFile, pszBuf, STRLEN( pszBuf ) );
      if ( 0 == nResult ) {
         FREE(pszBuf);
         return EFAILED;
      }
   }

   FREE( pszBuf );

   return SUCCESS;
}



/*======================================================================= 
Function: Loc_SaveGPSSettings()

Description: 
   Opens the configuration file and saves the settings.
=======================================================================*/
uint32 Loc_SaveGPSSettings( IShell *pIShell )
{
   IFileMgr   *pIFileMgr = NULL;
   IFile      *pIConfigFile = NULL;
   uint32      nResult = 0;

   if (pIShell == NULL)
      return EFAILED;
   
   // Create the instance of IFileMgr
   nResult = ISHELL_CreateInstance( pIShell, AEECLSID_FILEMGR, (void**)&pIFileMgr );
   if ( SUCCESS != nResult ) {
      return nResult;
   }

   // If the config file exists, open it and read the settings.  Otherwise, we need to
   // create a new config file.
   nResult = IFILEMGR_Test( pIFileMgr, LOC_CONFIG_FILE );
   if ( SUCCESS == nResult ) {
      pIConfigFile = IFILEMGR_OpenFile( pIFileMgr, LOC_CONFIG_FILE, _OFM_READWRITE );
      if ( !pIConfigFile ) {
         nResult = EFAILED;
      }
      else {
		 // Setup the default GPS settings
		 AEEGPSConfig *gpsConfig = (AEEGPSConfig*)MALLOC(sizeof(AEEGPSConfig));
		 if (gpsConfig != NULL)
		 {
			gpsConfig->optim  = AEEGPS_OPT_DEFAULT;
			gpsConfig->qos    = LOC_QOS_DEFAULT;
			gpsConfig->server.svrType = AEEGPS_SERVER_DEFAULT;
			nResult = Loc_WriteGPSSettings( pIConfigFile , gpsConfig);
			FREE(gpsConfig);
		 }
         else
		 {
			 nResult = EFAILED;
		 }
      }
   }
   else {
      pIConfigFile = IFILEMGR_OpenFile( pIFileMgr, LOC_CONFIG_FILE, _OFM_CREATE );
      if ( !pIConfigFile ) {
         nResult = EFAILED;
      }
      else {
         // Setup the default GPS settings
		 AEEGPSConfig *gpsConfig = (AEEGPSConfig*)MALLOC(sizeof(AEEGPSConfig));
		 if (gpsConfig != NULL)
		 {
			gpsConfig->optim  = AEEGPS_OPT_DEFAULT;
			gpsConfig->qos    = LOC_QOS_DEFAULT;
			gpsConfig->server.svrType = AEEGPS_SERVER_DEFAULT;
			nResult = Loc_WriteGPSSettings( pIConfigFile , gpsConfig);
			FREE(gpsConfig);
		 }
         else
		 {
			 nResult = EFAILED;
		 }
      }
   }

   // Free the IFileMgr and IFile instances
   IFILE_Release( pIConfigFile );
   IFILEMGR_Release( pIFileMgr );

   return nResult;
}

void Loc_Test_All(void)
{
   Coordinate c1, c2;
   double dis = 0, th = 0;
   char szDis[64], szAzh[64];
   AECHAR bufDis[32], bufAzh[32];

   //DESTINATION
   //beijing 39.911954, 116.377817
   c1.lat = 39.911954;
   c1.lon = 116.377817;

   //1 chengde 40.8493953666,118.0478839599
   c2.lat = 40.8493953666;
   c2.lon = 118.0478839599;
   dis = Loc_Calc_Distance(c1.lat, c1.lon, c2.lat, c2.lon);
   th = Loc_Calc_Azimuth(c1.lat, c1.lon, c2.lat, c2.lon);

   MEMSET(szDis,0,sizeof(szDis));
   FLOATTOWSTR(dis, bufDis, 32);
   WSTRTOSTR(bufDis,szDis, 64);

   MEMSET(szAzh,0,sizeof(szAzh));
   FLOATTOWSTR(th, bufAzh, 32);
   WSTRTOSTR(bufAzh, szAzh, 64);
   
   DBGPRINTF("location_get 1 dis:%s azh:%s", szDis, szAzh);

   //2 shanghai 31.1774276, 121.5272106	//1076679.0804040465
   c2.lat = 31.1774276;
   c2.lon = 121.5272106;
   dis = Loc_Calc_Distance(c1.lat, c1.lon, c2.lat, c2.lon);
   th = Loc_Calc_Azimuth(c1.lat, c1.lon, c2.lat, c2.lon);

   MEMSET(szDis,0,sizeof(szDis));
   FLOATTOWSTR(dis, bufDis, 32);
   WSTRTOSTR(bufDis,szDis, 64);

   MEMSET(szAzh,0,sizeof(szAzh));
   FLOATTOWSTR(th, bufAzh, 32);
   WSTRTOSTR(bufAzh, szAzh, 64);   
   DBGPRINTF("location_get 2 dis:%s azh:%s", szDis, szAzh);


   //3 sjz 38.0422378880,114.4925141047
   c2.lat = 38.0422378880;
   c2.lon = 114.4925141047;
   dis = Loc_Calc_Distance(c1.lat, c1.lon, c2.lat, c2.lon);
   th = Loc_Calc_Azimuth(c1.lat, c1.lon, c2.lat, c2.lon);

   MEMSET(szDis,0,sizeof(szDis));
   FLOATTOWSTR(dis, bufDis, 32);
   WSTRTOSTR(bufDis,szDis, 64);

   MEMSET(szAzh,0,sizeof(szAzh));
   FLOATTOWSTR(th, bufAzh, 32);
   WSTRTOSTR(bufAzh, szAzh, 64);
   DBGPRINTF("location_get 3 dis:%s azh:%s", szDis, szAzh);


   //4 zhangjiakou 40.3964667463,114.8377011418
   c2.lat = 40.3964667463;
   c2.lon = 114.8377011418;
   dis = Loc_Calc_Distance(c1.lat, c1.lon, c2.lat, c2.lon);
   th = Loc_Calc_Azimuth(c1.lat, c1.lon, c2.lat, c2.lon);

   MEMSET(szDis,0,sizeof(szDis));
   FLOATTOWSTR(dis, bufDis, 32);
   WSTRTOSTR(bufDis,szDis, 64);

   MEMSET(szAzh,0,sizeof(szAzh));
   FLOATTOWSTR(th, bufAzh, 32);
   WSTRTOSTR(bufAzh, szAzh, 64);
   DBGPRINTF("location_get 4 dis:%s azh:%s", szDis, szAzh);

}



