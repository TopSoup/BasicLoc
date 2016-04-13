/*===============================================================================
INCLUDES AND VARIABLE DEFINITIONS
=============================================================================== */
#include "AEEStdLib.h"
#include "AEEAppGen.h"        // Applet helper file
#include "AEEWeb.h"
#include "AEEFile.h"

#include "BasicLoc.bid"		// Applet-specific header that contains class ID
#include "nmdef.h"

#include "Location.h"

#define	BASIC_LOC_VERSION	"1.0.0.2-20160413"

#ifdef AEE_SIMULATOR
#	define LOG_FILE_PATH				"fs:/shared/basicloc.log"
#	define CONFIG_PATH					"fs:/shared/config.txt"
#else
#	define LOG_FILE_PATH				"fs:/mod/basicloc/basicloc.log"
#	define CONFIG_PATH					"fs:/mod/basicloc/config.txt"
#endif // AEE_SIMULATOR

/************************************************************************/
/* CONFIG ITEM                                                          */
/************************************************************************/
#define CONFIG_SVR_IP		"ip="		//服务器IP
#define CONFIG_SVR_PORT		"port="		//服务器端口
#define CONFIG_INTERVAL		"interval="	//定位上传间隔

#define DEFAULT_INTERVAL	30			//默认30秒间隔
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
#define WATCHER_TIMER	5
#define NET_TIMER		5
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
	uint32				m_nIdle;				//空闲状态
	
	//Server Config
	char				m_szIP[20];				//服务器IP
	uint16				m_nPort;				//服务器端口
	uint16				m_interval;				//上传间隔

	char				m_szData[255];			//数据内容
	uint16				m_nLen;					//数据长度

	//File 
	IFileMgr*			m_fm;
	IFile*				m_file;

	//Device
	char				m_meid[32];				//设备MEID

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

//Get MEID
static int		CBasicLoc_GetMeid(CBasicLoc *pme);

//Load Config
static uint32	LoadConfig(CBasicLoc *pme);

//格式化浮点 纬度:DDDMM.MMMM , 经度:DDMM, MMMMM
static char* FORMATFLT(char* szBuf, double val);


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

int hal_snprintf(char* s, size_t n, const char* format, ...) {
	int ret;
	va_list argp;
	va_start(argp, format);
	ret = VSNPRINTF(s, n, format, argp);
	va_end(argp);
	return ret;
}
char* hal_walltime_string(char* buffer, size_t size) {
	uint32 secs;
	int len;
	JulianType julian;


	if (NULL == buffer || 0 == size)
		return NULL;
	secs = GETTIMESECONDS();
	GETJULIANDATE(secs, &julian);

	len = hal_snprintf(buffer, size, "%04d/%02d/%02d %02d:%02d:%02d", julian.wYear, julian.wMonth, julian.wDay,
		julian.wHour, julian.wMinute, julian.wSecond);

	if (len)
		return buffer;
	return NULL;
}

int	log_output(IFile* file, char* szLog)
{
	char szBuffer[1024];
	char szTime[32];

	if (file == NULL)
		return  -1;

	hal_walltime_string(szTime, 32);

	SPRINTF(szBuffer, "%s %s\n", szTime, szLog);
	IFILE_Write(file, szBuffer, STRLEN(szBuffer));
	
	DBGPRINTF("%s", szBuffer);

	return 0;
}

/*===========================================================================
This function for play tts.
===========================================================================*/
void play_tts(CBasicLoc *pme, AECHAR* wtxt)
{
	uint16 len = 0;

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

//Get MEID
static int CBasicLoc_GetMeid(CBasicLoc *pme)
{
	char szBuf[64];
	int size = 0;
	int err = 0;

	MEMSET(pme->m_meid, 0, 32);

	size = 32;
	err = ISHELL_GetDeviceInfoEx(pme->a.m_pIShell, AEE_DEVICEITEM_MEIDS, pme->m_meid, &size);
	DBGPRINTF("meid: %s size:%d err:%d", pme->m_meid, size, err);
	if (err == SUCCESS)
	{
		SPRINTF(szBuf, "Get MEID: %s\n", pme->m_meid);
		log_output(pme->m_file, szBuf);
	}
	else
	{
		SPRINTF(szBuf, "Get MEID Failed! ERR:%d", err);
		log_output(pme->m_file, szBuf);
	}

	return err;
}

static boolean CBasicLoc_InitAppData(CBasicLoc *pme)
{
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	pme->m_gpsMode = AEEGPS_MODE_TRACK_STANDALONE;

	pGetGPSInfo->bAbort = TRUE;

	//FileMgr
	if (AEE_SUCCESS != ISHELL_CreateInstance(pme->a.m_pIShell, AEECLSID_FILEMGR, (void **)&(pme->m_fm))) {
		DBGPRINTF("ISHELL_CreateInstance for AEECLSID_FILEMGR failed!");
		return FALSE;
	}

#if 0
	//Delete Old Log
	if (AEE_SUCCESS == IFILEMGR_Test(pme->m_fm, LOG_FILE_PATH))
	{
		IFILEMGR_Remove(pme->m_fm, LOG_FILE_PATH);
	}
#endif

	//Open File
	pme->m_file = IFILEMGR_OpenFile(pme->m_fm, LOG_FILE_PATH, _OFM_APPEND);
	if (NULL == pme->m_file)
	{
		pme->m_file = IFILEMGR_OpenFile(pme->m_fm, LOG_FILE_PATH, _OFM_CREATE);
		if (NULL == pme->m_file)
		{
			DBGPRINTF("IFILEMGR_OpenFile %s failed!", LOG_FILE_PATH);
			return FALSE;
		}
	}

	//Get MEID
	CBasicLoc_GetMeid(pme);

	//Load Config
	if (SUCCESS != LoadConfig(pme))
	{
		DBGPRINTF("Please Config Server info!");
		log_output(pme->m_file, "Please Configure Server Information");
		play_tts(pme, L"Please Configure Server Information");
		
	}

	//Callback
	CALLBACK_Init(&pme->m_cbWatcherTimer, CBasicLoc_GetGPSInfo_Watcher, pme);
	ISHELL_SetTimerEx(pme->a.m_pIShell, 10000, &pme->m_cbWatcherTimer);

	CALLBACK_Init(&pme->m_cbNetTimer, CBasicLoc_Net_Timer, pme);
	ISHELL_SetTimerEx(pme->a.m_pIShell, 8000, &pme->m_cbNetTimer);

	//ONLY FOR TEST

// 	{
// 		char szBuf[64];
// 
// 		DBGPRINTF("@@114.123456 %s", FORMATFLT(szBuf, 114.123456));
// 		DBGPRINTF("@@-27.654321 %s", FORMATFLT(szBuf, -27.654321));
// 		DBGPRINTF("@@-0.123456 %s", FORMATFLT(szBuf, -0.123456));
// 		DBGPRINTF("@@1.654321 %s", FORMATFLT(szBuf, 1.654321));
// 
// 	}

	//get devices id
// 	{
// 		char szBuf[128];
// 		char esn[64], meid[64], imsi[64], imei[64];
// 		int size = 0;
// 		int err = 0;
// 
// 		size = 32;
// 		err = ISHELL_GetDeviceInfoEx(pme->a.m_pIShell, AEE_DEVICEITEM_MEIDS, meid, &size);
// 		DBGPRINTF("meid: %s size:%d err:%d", meid, size, err);
// 
// 		size = 32;
// 		err = ISHELL_GetDeviceInfoEx(pme->a.m_pIShell, AEE_DEVICEITEM_ESN, esn, &size);
// 		DBGPRINTF("esn: %s size:%d err:%d", esn, size, err);
// 
// 		size = 32;
// 		err = ISHELL_GetDeviceInfoEx(pme->a.m_pIShell, AEE_DEVICEITEM_MOBILE_ID, imsi, &size);
// 		DBGPRINTF("imsi: %s size:%d err:%d", imsi, size, err);
// 
// 		size = 32;
// 		err = ISHELL_GetDeviceInfoEx(pme->a.m_pIShell, AEE_DEVICEITEM_IMEI, imei, &size);
// 		DBGPRINTF("imei: %s size:%d err:%d", imei, size, err);
// 
// 		SPRINTF(szBuf, "device_id esn:%s meid:%s imsi:%s imei:%s", esn, meid, imsi, imei);
// 		log_output(pme->m_file, szBuf);
// 
// 	}


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

	BL_RELEASEIF(pme->m_file);
	BL_RELEASEIF(pme->m_fm);
}

#if defined(AEE_STATIC)
extern "C" int CBasicLoc_CreateInstance(AEECLSID ClsId, IShell * pIShell, IModule * pMod, void ** ppObj)

extern "C" int CBasicLoc_Load(IShell * ps, void * pHelpers, IModule ** pMod) {
	return AEEStaticMod_New(sizeof(AEEMod), ps, pHelpers, pMod, CBasicLoc_CreateInstance, NULL);
}

extern "C" int CBasicLoc_CreateInstance(AEECLSID ClsId, IShell * pIShell, IModule * po, void ** ppObj)
#else
int AEEClsCreateInstance(AEECLSID ClsId, IShell * pIShell, IModule * pMod, void ** ppObj)
#endif
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
		DBGPRINTF(BASIC_LOC_VERSION);
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

			if (pme->m_gpsMode == AEEGPS_MODE_TRACK_STANDALONE)
				play_tts(pme, L"Standalone mode");
			else
				play_tts(pme, L"default mode");

			CBasicLoc_LocStop(pme);
			CBasicLoc_LocStart(pme);
		}
		return(TRUE);
	}
	case EVT_NOTIFY:
	{
		AEENotify *wp = (AEENotify *)dwParam;
		DBGPRINTF("receive notify cls=%x", wp->cls);

		//开机自启动
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
	pGetGPSInfo->theInfo.gpsConfig.nInterval = pme->m_interval;

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

		//上报定位数据
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

	//重新启动
	//1 空闲30秒
	//2 尝试3分钟未定位成功
	if (pGetGPSInfo->wIdleCount > WATCHER_TIMER || pGetGPSInfo->wProgress > 60 * 3)
	{
		//play_tts(pme, L"restart location");

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
	//一小时回环
	pme->m_nIdle = (pme->m_nIdle + 1) % 3600;

	//查看网络是否初始化(20秒)
	if (pme->m_nIdle > NET_TIMER)
	{
		//初始化网络管理模块
		if (pme->m_pINetMgr == NULL)
		{
			if (ISHELL_CreateInstance(pme->a.m_pIShell, AEECLSID_NET, (void**)(&pme->m_pINetMgr)) != SUCCESS)
			{
				play_tts(pme, L"Create Network Manager Failed!");
				DBGPRINTF("Create Network Manager Failed!");
				pme->m_nIdle = 0;	//等待30秒继续初始化NETMGR
			}
		}
		
		//初始化网络接口
		if (pme->m_pINetMgr != NULL && pme->m_pISocket == NULL)
		{
			pme->m_pISocket = INETMGR_OpenSocket(pme->m_pINetMgr, AEE_SOCK_DGRAM);

			if (pme->m_pISocket == NULL) {
				play_tts(pme, L"OpenSocket Failed!");
				DBGPRINTF("OpenSocket Failed!");
				pme->m_nIdle = 0;	//等待30秒继续初始化SOCKET
			}

			//设定服务器端口和IP
			//STRCPY(pme->m_szIP, "119.254.211.165");
			//pme->m_nPort = 10061;


			//STRCPY(pme->m_szIP, "60.195.250.128");
			//pme->m_nPort = 6767;

			//STRCPY(pme->m_szData, "THIS IS TEST WORD.");
			//pme->m_nLen = STRLEN(pme->m_szData) + 1;

		}
	}

	ISHELL_SetTimerEx(pme->a.m_pIShell, 1000, &pme->m_cbNetTimer);
}

//格式化浮点 纬度:DDDMM.MMMM , 经度:DDMM, MMMMM
static char* FORMATFLT(char* szBuf, double val)
{
	double tmp = 0, tt = 0, min = 0;
	int d = 0, m = 0;
	int zero_pad = 0;
	char szZero[16];

	if (szBuf == NULL)
		return NULL;

	tmp = FABS(val);
	if (FCMP_GE(tmp, 0.000001))
	{
		tt = FFLOOR(tmp);
		min = FSUB(tmp, tt);
		min = FMUL(min, 60.0);
		min = FDIV(min, 100.0);
		tmp = FADD(tt, min);

		//取出放大100倍后的整数部分和小数部分
		//tmp = FMUL(FABS(val), 100);
		tmp = FMUL(tmp, 100);
		tt = FFLOOR(tmp);
		d = FLTTOINT(tt);
		m = FLTTOINT(FMUL(FSUB(tmp, tt), 100000.0));

		//四舍五入
		m = (m % 10 >= 5) ? (m + 10) / 10 : m / 10;

		if (m > 0)
		{	
			if (m < 1000)	//0.0123
			{
				zero_pad++;
			}
			
			if (m < 100)	//0.0012
			{
				zero_pad++;
			}
			
			if (m < 10)		//0.0001
			{
				zero_pad++;
			}
			
			//补充后面的0
			if (zero_pad > 0)
			{
				STRNCPY(szZero, "0000", zero_pad);
				szZero[zero_pad] = 0;
			}
		}
	}
	else
	{
			d = 0;
			m = 0;
	}

	if (zero_pad > 0)
		SPRINTF(szBuf, "%d.%s%d", d, szZero, m);
	else
		SPRINTF(szBuf, "%d.%d", d, m);

	return szBuf;
}

//发送定位数据
//*EX,2100428040,MOVE,053651,A,2945.7672,N,12016.8198,E,0.00,000,180510,FBFFFFFF#
static void CBasicLoc_UDPWrite(CBasicLoc *pme)
{
	int nErr;
	int32 nIP = 0;
	int16 nPort = 0;
	char deviceID[20];
	char location[64];
	char szLat[32], szLon[32];
	char date[16];

	JulianType julian;
	uint32 secs = 0;
	GetGPSInfo *pGetGPSInfo = &pme->m_gpsInfo;

	if (pme->m_pISocket == NULL || pme->m_pINetMgr == NULL)
	{
		return;
	}

	if (STRLEN(pme->m_szIP) == 0 || pme->m_nPort == 0)
	{
		play_tts(pme, L"Please Configure Server Infomation");
		DBGPRINTF("Please Configure Server Infomation");
		return;
	}
	
	//如果设备编号不存在,尝试重新读取一次
	if (STRLEN(pme->m_meid) == 0)
	{
#ifdef AEE_SIMULATOR
		STRCPY(pme->m_meid, "A000003841A719");
#else
		//Get MEID
		if (SUCCESS != CBasicLoc_GetMeid(pme))
		{
			play_tts(pme, L"NO MEID!");
			return;
		}
#endif
	}

	/* 填充协议 */
	//STRCPY(deviceID, "A000003841A7190");
	STRCPY(deviceID, pme->m_meid);

	//FOR TEST
#if 0
	//39.9091407478,116.3975900499
	//39.9151600000,116.4039570000
	//38.0428043266,114.5143068110
	pGetGPSInfo->theInfo.lat = 38.0428043266;//39.9151600000;//38.0472833266;//38.1015619154;
	pGetGPSInfo->theInfo.lon = 114.5143068110;//116.4039570000;//114.5117308110;//114.6364600054;
#endif

	FORMATFLT(szLat, pGetGPSInfo->theInfo.lat);
	FORMATFLT(szLon, pGetGPSInfo->theInfo.lon);
	DBGPRINTF("@Lat: %s", szLat);
	DBGPRINTF("@Lon: %s", szLon);

	//位置信息
	if (FCMP_G(pGetGPSInfo->theInfo.lat, 0) && FCMP_G(pGetGPSInfo->theInfo.lon, 0))
	{
		SPRINTF(location, "A,%s,N,%s,E,0.00,000", szLat, szLon);

		play_tts(pme, L"locate success!");
	}
	else
	{
		SPRINTF(location, "V,0000.0000,N,00000.0000,E,0.00,000");

		play_tts(pme, L"locate invalid!");
	}

	if (pme->m_file)
	{
		log_output(pme->m_file, location);
	}

	//时间
	secs = GETTIMESECONDS();
	GETJULIANDATE(secs, &julian);
	SPRINTF(date, "%02d%02d16", julian.wDay, julian.wMonth);

	SPRINTF(pme->m_szData, "*EX,%s,MOVE,053651,%s,%s,FBFFFFFF#", deviceID, location, date);
	pme->m_nLen = STRLEN(pme->m_szData) + 1;

	DBGPRINTF("@Report: %s", pme->m_szData);

	INET_ATON(pme->m_szIP, (uint32 *)&nIP);
	nPort = HTONS(pme->m_nPort);

	nErr = ISOCKET_SendTo(pme->m_pISocket, (byte *)pme->m_szData, pme->m_nLen, 0, nIP, nPort);

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


/************************************************************************/
/* Get Server Configure                                                                     */
/************************************************************************/

static int DistToSemi(const char * pszStr)
{
	int nCount = 0;
	if (!pszStr)
		return -1;

	while (*pszStr != 0) {
		if (*pszStr == ';' || (*pszStr != '.' && (*pszStr < 0x30 || *pszStr > 0x39))) {
			return nCount;
		}
		else {
			nCount++;
			pszStr++;
		}
	}
	return -1;
}

int TrimSpace(char *inbuf, char *outbuf)
{
	char *tmpBuf = inbuf;
	while (*tmpBuf != '\0')
	{
		if ((*tmpBuf) != ' '){
			*outbuf++ = *tmpBuf;
		}
		tmpBuf++;
	}
	*outbuf = '\0';
	return 0;
}

/************************************************************************/
/* 加载配置文件，读取服务器及上传配置                                   */
/************************************************************************/
static uint32 LoadConfig(CBasicLoc *pme)
{
	IFileMgr	*pIFileMgr = NULL;
	IFile		*pIFile = NULL;
	IShell		*pIShell = NULL;

	char    *pszBufOrg = NULL;
	char    *pszBuf = NULL;
	char    *pszTok = NULL;
	char    *pszDelimiter = ";";
	int32	nResult = 0;
	FileInfo	fiInfo;
	
	pIShell = pme->a.m_pIShell;

	// Create the instance of IFileMgr
	nResult = ISHELL_CreateInstance(pIShell, AEECLSID_FILEMGR, (void**)&pIFileMgr);
	if (SUCCESS != nResult) {
		return nResult;
	}

	nResult = IFILEMGR_Test(pIFileMgr, CONFIG_PATH);
	if (nResult != SUCCESS)
	{
		DBGPRINTF("CONFIG NOT EXIST!");
		IFILEMGR_Release(pIFileMgr);
		return SUCCESS;
	}

	pIFile = IFILEMGR_OpenFile(pIFileMgr, CONFIG_PATH, _OFM_READWRITE);
	if (!pIFile) {
		DBGPRINTF("Open Configure File Failed! %s", CONFIG_PATH);
		IFILEMGR_Release(pIFileMgr);
	}

	if (SUCCESS != IFILE_GetInfo(pIFile, &fiInfo)) {
		IFILE_Release(pIFile);
		IFILEMGR_Release(pIFileMgr);
		return EFAILED;
	}

	if (fiInfo.dwSize == 0) {
		IFILE_Release(pIFile);
		IFILEMGR_Release(pIFileMgr);
		return EFAILED;
	}

	// Allocate enough memory to read the full text into memory
	pszBufOrg = MALLOC(fiInfo.dwSize);
	pszBuf = MALLOC(fiInfo.dwSize);

	nResult = IFILE_Read(pIFile, pszBufOrg, fiInfo.dwSize);
	if ((uint32)nResult < fiInfo.dwSize) {
		FREE(pszBuf);
		return EFAILED;
	}

	TrimSpace(pszBufOrg, pszBuf);
	FREE(pszBufOrg);

	//server port
	pszTok = STRSTR(pszBuf, CONFIG_SVR_PORT);
	if (pszTok) {
		pszTok = pszTok + STRLEN(CONFIG_SVR_PORT);
		pme->m_nPort = (uint16)STRTOUL(pszTok, &pszDelimiter, 10);
	}
	else
	{
		FREE(pszBuf);
		IFILE_Release(pIFile);
		IFILEMGR_Release(pIFileMgr);
		return EFAILED;
	}

	//server ip
	pszTok = STRSTR(pszBuf, CONFIG_SVR_IP);
	if (pszTok) {
		pszTok = pszTok + STRLEN(CONFIG_SVR_IP);
		nResult = DistToSemi(pszTok);
		if (nResult < 7)	//IP至少为0.0.0.0
		{
			DBGPRINTF("Not Found IP");

			FREE(pszBuf);
			IFILE_Release(pIFile);
			IFILEMGR_Release(pIFileMgr);
			return EFAILED;
		}
		else
		{
			STRNCPY(pme->m_szIP, pszTok, nResult);
		}
		pme->m_szIP[nResult] = 0;
	}
	else
	{
		FREE(pszBuf);
		IFILE_Release(pIFile);
		IFILEMGR_Release(pIFileMgr);
		return EFAILED;
	}

	//interval
	pszTok = STRSTR(pszBuf, CONFIG_INTERVAL);
	if (pszTok) {
		pszTok = pszTok + STRLEN(CONFIG_INTERVAL);
		pme->m_interval = (uint16)STRTOUL(pszTok, &pszDelimiter, 10);
	}
#ifndef AEE_SIMULATOR
	if (pme->m_interval < DEFAULT_INTERVAL)
	{
		pme->m_interval = DEFAULT_INTERVAL;
	}
#endif
	FREE(pszBuf);
	IFILE_Release(pIFile);
	IFILEMGR_Release(pIFileMgr);

	return SUCCESS;
}
