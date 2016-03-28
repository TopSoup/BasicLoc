/*===============================================================================
INCLUDES AND VARIABLE DEFINITIONS
=============================================================================== */
#include "AEEStdLib.h"
#include "AEEAppGen.h"        // Applet helper file
#include "AEEWeb.h"
#include "BasicLoc.bid"		// Applet-specific header that contains class ID
#include "nmdef.h"

#include "Location.h"

/*===========================================================================

FOR ZTE G180 MACROS DECLARATIONS

===========================================================================*/
//ZTE CLSID
#define AEECLSID_ZTE_APP			0x01007100
#define AEECLSID_ZTEAPPCORE			(AEECLSID_ZTE_APP + 0x1)
#define AEECLSID_ZTE_TTSSVC			0x01007A04

//EVENT
#define EVT_POC				0x8000		// TTS
#define EVT_POC_POWEROFF	0x8001		// POWEROFF
#define EVT_POC_BATTERY		0x8002		// BATTERY

//KEY
#define AVK_PTT_G180		0xE082
#define AVK_PLUS_G180		0xE047
#define AVK_DEC_G180		0xE048
#define AVK_LONGPOWER_G180	0xE02D
#define AVK_POWER_G180		0xE02E

/************************************************************************/
/* TIMER CONTROL                                                        */
/************************************************************************/
#ifdef AEE_SIMULATOR
#define WATCHER_TIMER	15
#define NET_TIMER		10
#else
#define WATCHER_TIMER	30
#define NET_TIMER		20
#endif
#define BL_RELEASEIF(p) BL_FreeIF((IBase **)&(p))

typedef struct _CBasicLoc
{
	AEEApplet		a;

	//Location Services
	AEEGPSMode		m_gpsMode;	//gpsMode
	GetGPSInfo		m_gpsInfo;	//gpsInfo
	AEECallback		m_cbWatcherTimer;


	//NetWork Services
	INetMgr *			m_pINetMgr;             // Pointer to INetMgr
	AEECallback			m_cbNetTimer;	        // Callback for NetWork
	ISocket *			m_pISocket;             // Pointer to socket
	uint32				m_nIdle;				//����״̬
	
	char				m_szIP[20];				//������IP
	uint16				m_nPort;				//�������˿�
	char				m_szData[255];			//��������
	uint16				m_nLen;					//���ݳ���

}CBasicLoc;

/*-------------------------------------------------------------------
Static function prototypes
-------------------------------------------------------------------*/
static boolean	CBasicLoc_InitAppData(CBasicLoc *pme);
static void		CBasicLoc_FreeAppData(CBasicLoc *pme);
static boolean	CBasicLoc_HandleEvent(CBasicLoc *pme, AEEEvent eCode, uint16 wParam, uint32 dwParam);

static void		CBasicLoc_LocStart(CBasicLoc *pme);
static void		CBasicLoc_LocStop(CBasicLoc *pme);
static void		CBasicLoc_GetGPSInfo_Watcher(CBasicLoc *pme);
static void		CBasicLoc_GetGPSInfo_Callback(CBasicLoc *pme);
static void		CBasicLoc_Net_Timer(CBasicLoc *pme);
static void		CBasicLoc_UDPWrite(CBasicLoc *pme);

/*===============================================================================
FUNCTION DEFINITIONS
=============================================================================== */

/*===========================================================================
This function releases IBase.
===========================================================================*/
void BL_FreeIF(IBase ** ppif)
{
	if (ppif && *ppif)
	{
		IBASE_Release(*ppif);
		*ppif = NULL;
	}
}

/*===========================================================================
This function for play tts.
===========================================================================*/
void play_tts(CBasicLoc *pme, AECHAR* wtxt)
{
	int len = 0;

	DBGPRINTF("play_tts in");
	if (wtxt == NULL)
	{
		DBGPRINTF("Error Input TTS text");
	}

	len = WSTRLEN(wtxt) + 1;
	DBGPRINTF("play_tts len:%d", len);
	if (!ISHELL_SendEvent(pme->a.m_pIShell, AEECLSID_ZTEAPPCORE, EVT_POC, len, (uint32)(wtxt))) {
		DBGPRINTF("PlayTTS ISHELL_SendEvent EVT_POC failure");
	}
}

static boolean CBasicLoc_InitAppData(CBasicLoc *pme)
{
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	pme->m_gpsMode = AEEGPS_MODE_TRACK_OPTIMAL;

	pGetGPSInfo->bAbort = TRUE;

	CALLBACK_Init(&pme->m_cbWatcherTimer, CBasicLoc_GetGPSInfo_Watcher, pme);
	ISHELL_SetTimerEx(pme->a.m_pIShell, 10000, &pme->m_cbWatcherTimer);

	CALLBACK_Init(&pme->m_cbNetTimer, CBasicLoc_Net_Timer, pme);
	ISHELL_SetTimerEx(pme->a.m_pIShell, 8000, &pme->m_cbNetTimer);

	return TRUE;
}

static void CBasicLoc_FreeAppData(CBasicLoc *pme)
{
	CBasicLoc_LocStop(pme);

	CALLBACK_Cancel(&pme->m_cbWatcherTimer);
	CALLBACK_Cancel(&pme->m_cbNetTimer);

	if (pme->m_pISocket != NULL)
	{
		ISOCKET_Close(pme->m_pISocket);
		BL_RELEASEIF(pme->m_pISocket);
	}

	BL_RELEASEIF(pme->m_pINetMgr);
}

int AEEClsCreateInstance(AEECLSID ClsId, IShell * pIShell, IModule * pMod, void ** ppObj)
{
	*ppObj = NULL;

	//
	// Here a check is done to see if the ClsID is that of navigate app.
	// The reason is if this module has more than one applets or classes, then this function is invoked
	// once for each applet or class. Checking here ensures that the correct IApplet or class object is
	// constructed.
	//
	if (ClsId == AEECLSID_BASICLOC)
	{
		//Create the applet
		if (AEEApplet_New(sizeof(CBasicLoc), ClsId, pIShell, pMod, (IApplet**)ppObj, (AEEHANDLER)CBasicLoc_HandleEvent, (PFNFREEAPPDATA)CBasicLoc_FreeAppData))
		{
			//Initialize applet data
			if (CBasicLoc_InitAppData((CBasicLoc*)*ppObj))
			{
				//Data initialized successfully
				return(AEE_SUCCESS);
			}
			else
			{
				//Release the applet. This will free the memory allocated for the applet when
				*ppObj = NULL;
				IAPPLET_Release((IApplet*)*ppObj);
				return EFAILED;
			}

		}//AEEApplet_New

	}// ClsId == AEECLSID_NAVIGATE

	return(EFAILED);
}

/*===========================================================================
===========================================================================*/
static boolean CBasicLoc_HandleEvent(CBasicLoc * pme, AEEEvent eCode, uint16 wParam, uint32 dwParam)
{
	switch (eCode){
	case EVT_APP_START:
		DBGPRINTF("EVT_APP_START");
		play_tts(pme, L"start");
		return(TRUE);

	case EVT_APP_STOP:
		DBGPRINTF("EVT_APP_STOP");
		return(TRUE);

	case EVT_KEY:
	{
		DBGPRINTF("EVT_KEY");
		//case EVT_KEY_PRESS:
		if (wParam == AVK_PTT_G180 || wParam == AVK_PTT)
		{
			DBGPRINTF("AVK_PTT %x", AVK_PTT_G180);
			pme->m_gpsMode = (pme->m_gpsMode == AEEGPS_MODE_TRACK_STANDALONE) ? AEEGPS_MODE_TRACK_OPTIMAL : AEEGPS_MODE_TRACK_STANDALONE;
			DBGPRINTF("Change gpsMode %x", pme->m_gpsMode);
			CBasicLoc_LocStop(pme);
			CBasicLoc_LocStart(pme);
		}
		return(TRUE);
	}
	case EVT_NOTIFY:
	{
		AEENotify *wp = (AEENotify *)dwParam;
		DBGPRINTF("receive notify cls=%x", wp->cls);

		//����������
		if (wp->cls == AEECLSID_SHELL)
		{
			if (wp->dwMask & NMASK_SHELL_INIT)
			{
				ISHELL_StartApplet(pme->a.m_pIShell, AEECLSID_BASICLOC);
			}
		}

		return (TRUE);
	}
	default:
		break;

	}
	return(FALSE);
}



/*===========================================================================
This function called by location modoule.
===========================================================================*/
static void CBasicLoc_LocStart(CBasicLoc *pme)
{
	int nErr = SUCCESS;
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	ZEROAT(pGetGPSInfo);

	pGetGPSInfo->theInfo.gpsConfig.server.svrType = AEEGPS_SERVER_DEFAULT;
	pGetGPSInfo->theInfo.gpsConfig.qos = 16;
	pGetGPSInfo->theInfo.gpsConfig.optim = 1;
	pGetGPSInfo->theInfo.gpsConfig.mode = pme->m_gpsMode;
	pGetGPSInfo->theInfo.gpsConfig.nFixes = 0;
	pGetGPSInfo->theInfo.gpsConfig.nInterval = 0;

	if (ISHELL_CreateInstance(pme->a.m_pIShell, AEECLSID_POSDET, (void **)&pGetGPSInfo->pPosDet) == SUCCESS) {

		CALLBACK_Init(&pGetGPSInfo->cbPosDet, CBasicLoc_GetGPSInfo_Callback, pme);

		nErr = Loc_Init(pme->a.m_pIShell, pGetGPSInfo->pPosDet, &pGetGPSInfo->cbPosDet, &pGetGPSInfo->pts);
		nErr = Loc_Start(pGetGPSInfo->pts, &pGetGPSInfo->theInfo);
		if (nErr != SUCCESS) {
			pGetGPSInfo->theInfo.nErr = nErr;
			DBGPRINTF("Loc_Start Failed! Err:%d", nErr);
		}
		else
		{
			pGetGPSInfo->bAbort = FALSE;
		}
	}
}


static void CBasicLoc_LocStop(CBasicLoc *pme)
{
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	if (pGetGPSInfo->pPosDet)
	{
		Loc_Stop(pGetGPSInfo->pts);

		CALLBACK_Cancel(&pGetGPSInfo->cbPosDet);
		BL_RELEASEIF(pGetGPSInfo->pPosDet);
	}
}


static void CBasicLoc_GetGPSInfo_Callback(CBasicLoc *pme)
{
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	DBGPRINTF("CBasicLoc_GetGPSInfo_Callback in nErr:%d", pGetGPSInfo->theInfo.nErr);

	if (pGetGPSInfo->theInfo.nErr == SUCCESS) {
		/* Process new data from IPosDet */
		pGetGPSInfo->dwFixNumber++;
		pGetGPSInfo->dwFixDuration += pGetGPSInfo->wProgress;
		pGetGPSInfo->wProgress = 0;
		pGetGPSInfo->wIdleCount = 0;
		DBGPRINTF("@GetGPSInfo fix:%d", pGetGPSInfo->dwFixNumber);

		//�ϱ���λ����
		CBasicLoc_UDPWrite(pme);
	}
	else if (pGetGPSInfo->theInfo.nErr == EIDLE) {
		/* End of tracking */
		DBGPRINTF("@End of tracking");
	}
	else if (pGetGPSInfo->theInfo.nErr == AEEGPS_ERR_TIMEOUT) {
		/* Record the timeout and perhaps re-try. */
		pGetGPSInfo->dwTimeout++;
	}
	else {

		CBasicLoc_LocStop(pme);
		DBGPRINTF("@Something is not right here. Requires corrective action. Bailout");

		/* Something is not right here. Requires corrective action. Bailout */
		pGetGPSInfo->bAbort = TRUE;
	}
}

/*===========================================================================
This function called by location modoule.
===========================================================================*/
static void CBasicLoc_GetGPSInfo_Watcher(CBasicLoc *pme)
{
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	if (pGetGPSInfo->bPaused == FALSE) {
		pGetGPSInfo->wProgress ++;
		DBGPRINTF("@Where GetGPS progress:%d", pGetGPSInfo->wProgress);
	}

	if (pGetGPSInfo->bAbort == TRUE)
	{
		pGetGPSInfo->wIdleCount ++;
		DBGPRINTF("@Where GetGPS wIdleCount:%d", pGetGPSInfo->wIdleCount);
	}

	//��������
	//1 ����30��
	//2 ����3����δ��λ�ɹ�
	if (pGetGPSInfo->wIdleCount > WATCHER_TIMER || pGetGPSInfo->wProgress > 60 * 3)
	{
		DBGPRINTF("@Where GetGPS CBasicLoc_LocStart");
		CBasicLoc_LocStop(pme);
		CBasicLoc_LocStart(pme);
	}

	ISHELL_SetTimerEx(pme->a.m_pIShell, 1000, &pme->m_cbWatcherTimer);
}

/*===========================================================================
This function called by basicloc modoule.
===========================================================================*/
static void CBasicLoc_Net_Timer(CBasicLoc *pme)
{
	//һСʱ�ػ�
	pme->m_nIdle = (pme->m_nIdle + 1) % 3600;

	//�鿴�����Ƿ��ʼ��(20��)
	if (pme->m_nIdle > NET_TIMER)
	{
		//��ʼ���������ģ��
		if (pme->m_pINetMgr == NULL)
		{
			if (ISHELL_CreateInstance(pme->a.m_pIShell, AEECLSID_NET, (void**)(&pme->m_pINetMgr)) != SUCCESS)
			{
				play_tts(pme, L"Create Network Manager Failed!");
				DBGPRINTF("Create Network Manager Failed!");
				pme->m_nIdle = 0;	//�ȴ�30�������ʼ��NETMGR
			}
		}
		
		//��ʼ������ӿ�
		if (pme->m_pINetMgr != NULL && pme->m_pISocket == NULL)
		{
			pme->m_pISocket = INETMGR_OpenSocket(pme->m_pINetMgr, AEE_SOCK_DGRAM);

			if (pme->m_pISocket == NULL) {
				play_tts(pme, L"OpenSocket Failed!");
				DBGPRINTF("OpenSocket Failed!");
				pme->m_nIdle = 0;	//�ȴ�30�������ʼ��SOCKET
			}

			//�趨�������˿ں�IP
			STRCPY(pme->m_szIP, "119.254.211.165");
			pme->m_nPort = 10061;

			STRCPY(pme->m_szData, "THIS IS TEST WORD.");
			pme->m_nLen = STRLEN(pme->m_szData) + 1;

		}
	}

	ISHELL_SetTimerEx(pme->a.m_pIShell, 1000, &pme->m_cbNetTimer);
}

//���Ͷ�λ����
//*EX,2100428040,MOVE,053651,A,2945.7672,N,12016.8198,E,0.00,000,180510,FBFFFFFF#
static void CBasicLoc_UDPWrite(CBasicLoc *pme)
{
	int nErr;
	int nIP = 0, nPort = 0;
	char deviceID[20];
	char location[64];
	char date[10];
	double	degree = 0, min = 0;

	JulianType julian;
	uint32 secs = 0;
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	if (pme->m_pISocket == NULL || pme->m_pINetMgr == NULL)
	{
		return;
	}

	/* ���Э�� */
	
	//�豸���
	STRCPY(deviceID, "A000003841A7190");

	//λ����Ϣ
	if (FCMP_G(pGetGPSInfo->theInfo.lat, 0) && FCMP_G(pGetGPSInfo->theInfo.lon, 0))
	{
		degree = FLTTOINT(pGetGPSInfo->theInfo.lat);
		min = FSUB(pGetGPSInfo->theInfo.lat, degree);
		SPRINTF(location, "A,0000,0000,N,0000,0000,E,0.00,000");
	}
	else
	{
		SPRINTF(location, "V,0000,0000,N,0000,0000,E,0.00,000");
	}

	//ʱ��
	secs = GETTIMESECONDS();
	GETJULIANDATE(secs, &julian);
	SPRINTF(date, "%02d%02d16", julian.wDay, julian.wMonth);



	INET_ATON(pme->m_szIP, (uint32 *)&nIP);
	nPort = HTONS(pme->m_nPort);

	nErr = ISOCKET_SendTo(pme->m_pISocket, pme->m_szData, pme->m_nLen, 0, nIP, nPort);

	if (nErr == AEE_NET_WOULDBLOCK) {
		DBGPRINTF("** sending...\n");
		ISOCKET_Writeable(pme->m_pISocket, (PFNNOTIFY)CBasicLoc_UDPWrite, pme);
	}
	else if (nErr == AEE_NET_ERROR) {
		DBGPRINTF("Send Failed: Error %d\n", ISOCKET_GetLastError(pme->m_pISocket));
		BL_RELEASEIF(pme->m_pISocket);
	}
	else {
		DBGPRINTF("** sending complete...\n");
		// Reset Buffer
		//MEMSET(pApp->m_pszMsg, 0, pApp->m_nDataLength);
		//Echoer_UDPRead(pApp);
	}
}
