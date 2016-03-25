/*===============================================================================
INCLUDES AND VARIABLE DEFINITIONS
=============================================================================== */
#include "AEEStdLib.h"
#include "AEEAppGen.h"        // Applet helper file
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



#define BL_RELEASEIF(p) BL_FreeIF((IBase **)&(p))

typedef struct _CBasicLoc
{
	AEEApplet		a;

	AEEGPSMode		m_gpsMode;	//gpsMode
	GetGPSInfo		m_gpsInfo;	//gpsInfo
	AEECallback		cbWatcherTimer;

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

	len = WSTRLEN(wtxt);
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

	CALLBACK_Init(&pme->cbWatcherTimer, CBasicLoc_GetGPSInfo_Watcher, pme);
	ISHELL_SetTimerEx(pme->a.m_pIShell, 1000, &pme->cbWatcherTimer);

	return TRUE;
}

static void CBasicLoc_FreeAppData(CBasicLoc *pme)
{
	CBasicLoc_LocStop(pme);

	CALLBACK_Cancel(&pme->cbWatcherTimer);
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
	if (pGetGPSInfo->wIdleCount > 30 || pGetGPSInfo->wProgress > 60*3)
	{
		DBGPRINTF("@Where GetGPS CBasicLoc_LocStart");
		CBasicLoc_LocStop(pme);
		CBasicLoc_LocStart(pme);
	}

	ISHELL_SetTimerEx(pme->a.m_pIShell, 1000, &pme->cbWatcherTimer);
}