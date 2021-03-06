#include "STCanScan.h"

//////////////////////////////////////////////////////////////////////
// strcan.cpp: implementation of the STCanScan class.
//////////////////////////////////////////////////////////////////////

#include <time.h>
#include <string.h>
#include "gettimeofday.h"

#include <LogIt.h>

//! The macro below is applicable only to this translation unit
#define MLOG(LEVEL,THIS) LOG(Log::LEVEL) << THIS->m_canName << " "
using namespace CanModule;

bool STCanScan::s_isCanHandleInUseArray[256];
tUcanHandle STCanScan::s_canHandleArray[256];

extern "C" __declspec(dllexport) CCanAccess *getCanbusAccess()
{
	CCanAccess *canAccess;
	canAccess = new STCanScan;
	return canAccess;
}

STCanScan::STCanScan():
m_CanScanThreadShutdownFlag(true),
m_moduleNumber(0),
m_channelNumber(0),
m_canHandleNumber(0),
m_busStatus(USBCAN_SUCCESSFUL),
m_baudRate(0)
{
	m_statistics.beginNewRun();
}

DWORD WINAPI STCanScan::CanScanControlThread(LPVOID pCanScan)
{
	BYTE status;
	tCanMsgStruct readCanMessage;
    STCanScan *canScanInstancePointer = reinterpret_cast<STCanScan *>(pCanScan);
    MLOG(DBG,canScanInstancePointer) << "CanScanControlThread Started. m_CanScanThreadShutdownFlag = [" << canScanInstancePointer->m_CanScanThreadShutdownFlag <<"]";
	while (canScanInstancePointer->m_CanScanThreadShutdownFlag)
	{
		status = UcanReadCanMsgEx(canScanInstancePointer->m_UcanHandle, (BYTE *)&canScanInstancePointer->m_channelNumber, &readCanMessage, 0);
		if (status == USBCAN_SUCCESSFUL)
		{
			CanMessage canMsgCopy;
			canMsgCopy.c_id = readCanMessage.m_dwID;
			canMsgCopy.c_dlc = readCanMessage.m_bDLC;
			canMsgCopy.c_ff = readCanMessage.m_bFF;
			for (int i = 0; i < 8; i++)
				canMsgCopy.c_data[i] = readCanMessage.m_bData[i];
			gettimeofday(&(canMsgCopy.c_time));
			canScanInstancePointer->canMessageCame(canMsgCopy);
			canScanInstancePointer->m_statistics.onReceive( readCanMessage.m_bDLC );
		}
		else
		{

			if (status == USBCAN_WARN_NODATA)
			{
				Sleep(100);
            }
			else
			{
				canScanInstancePointer->sendErrorCode(status);
			}
		}
	}
    ExitThread(0);
    return 0;
}

STCanScan::~STCanScan()
{
	MLOG(DBG,this) << "Closing down ST Can Scan component";
	//Shut down can scan thread
	m_CanScanThreadShutdownFlag = false;	
	DWORD result = WaitForSingleObject(m_hCanScanThread, INFINITE);
     // deinitialize CAN interface
    ::UcanDeinitCanEx (m_UcanHandle,(BYTE)m_channelNumber);
    MLOG(DBG,this) << "ST Can Scan component closed successfully";
}

bool STCanScan::createBUS(const char *name,const char *parameters)
{	
	int returnCode = configureCanboard(name, parameters);
	if (returnCode < 0)
	{
		return false;
	}

	//After the canboard is configured and started, we start the scan control thread
	m_hCanScanThread = CreateThread(NULL, 0, CanScanControlThread, this, 0, &m_idCanScanThread);
	if (NULL == m_hCanScanThread) {
		MLOG(ERR,this) << "Error when creating the canScanControl thread.";
		return false;
	}
	MLOG(DBG,this) << "Bus [" << name << "] created with parameters [" << parameters << "]";
	return true;
}

int STCanScan::configureCanboard(const char *name,const char *parameters)
{
	//The different parameters
	unsigned int paramTseg1 = 0;
	unsigned int paramTseg2 = 0;
	unsigned int paramSjw = 0;
	unsigned int paramNoSamp = 0;
	unsigned int paramSyncMode = 0;
	//BaudRate defined by the parameters
	long paramBaudRate;
	//Default BaudRate
	long baudRate = USBCAN_BAUD_125kBit;
	int	numberOfDetectedParameters;

	const char *na = strstr(name,":");
	m_canHandleNumber = atoi(na+1);

	m_moduleNumber = m_canHandleNumber/2;
	m_channelNumber = m_canHandleNumber%2;	
	MLOG(TRC,this) << "m_canHandleNumber:[" << m_canHandleNumber << "], m_moduleNumber:[" << m_moduleNumber << "], m_channelNumber:[" << m_channelNumber << "]"<< ", na[" << na << "," << na+1 << "]";

	if (strcmp(parameters, "Unspecified") != 0)

	{
		numberOfDetectedParameters = sscanf_s(parameters, "%d %d %d %d %d %d", &paramBaudRate, &paramTseg1, &paramTseg2, &paramSjw, &paramNoSamp, &paramSyncMode);

		/* Set baud rate to 125 Kbits/second.  */
		if (numberOfDetectedParameters >= 1)
		{
			switch (paramBaudRate)
			{
			case 50000: baudRate = USBCAN_BAUD_50kBit; break;
			case 100000: baudRate = USBCAN_BAUD_100kBit; break;
			case 125000: baudRate = USBCAN_BAUD_125kBit; break;
			case 250000: baudRate = USBCAN_BAUD_250kBit; break;
			case 500000: baudRate = USBCAN_BAUD_500kBit; break;
			case 1000000: baudRate = USBCAN_BAUD_1MBit; break;
			default: baudRate = paramBaudRate;
			}
		}
		else
		{
			MLOG(ERR, this) << "Error while parsing parameters: this syntax is incorrect: [" << parameters << "]";
			return -1;
		}
		m_baudRate = baudRate;
	}
	else
	{
		m_baudRate = baudRate;
		MLOG(DBG, this) << "Unspecified parameters, default values will be used.";
	}

	//We create an fill initializationParameters, to pass it to openCanPort
	tUcanInitCanParam   initializationParameters;
	initializationParameters.m_dwSize = sizeof(initializationParameters);           // size of this struct
	initializationParameters.m_bMode = kUcanModeNormal;              // normal operation mode
	initializationParameters.m_bBTR0 = HIBYTE(baudRate);              // baudrate
	initializationParameters.m_bBTR1 = LOBYTE(baudRate);
	initializationParameters.m_bOCR = 0x1A;                         // standard output
	initializationParameters.m_dwAMR = USBCAN_AMR_ALL;               // receive all CAN messages
	initializationParameters.m_dwACR = USBCAN_ACR_ALL;
	initializationParameters.m_dwBaudrate = USBCAN_BAUDEX_USE_BTR01;
	initializationParameters.m_wNrOfRxBufferEntries = USBCAN_DEFAULT_BUFFER_ENTRIES;
	initializationParameters.m_wNrOfTxBufferEntries = USBCAN_DEFAULT_BUFFER_ENTRIES;
	return openCanPort(initializationParameters);
}

int STCanScan::openCanPort(tUcanInitCanParam initializationParameters)
{
	BYTE systecCallReturn = USBCAN_SUCCESSFUL;
	//The Handle of the can Module
	tUcanHandle		canModuleHandle;

	// check if USB-CANmodul already is initialized
	if (isCanHandleInUse(m_moduleNumber))
	{
		//if it is, we get the handle
		canModuleHandle = getCanHandle(m_moduleNumber);
	}
	else
	{
		//Otherwise we create it.
		systecCallReturn = ::UcanInitHardwareEx(&canModuleHandle, m_moduleNumber, 0, 0);
		if (systecCallReturn != USBCAN_SUCCESSFUL)
		{
			// fill out initialisation struct
			MLOG(ERR,this) << "Error in UcanInitHardwareEx, return code = [" << systecCallReturn << "]";
			::UcanDeinitHardware(canModuleHandle);
			return -1;
		}
	}

	setCanHandleInUse(m_moduleNumber,true);
	// initialize CAN interface
	systecCallReturn = ::UcanInitCanEx2(canModuleHandle, m_channelNumber, &initializationParameters);
	if (systecCallReturn != USBCAN_SUCCESSFUL)
	{
		MLOG(ERR,this) << "Error in UcanInitCanEx2, return code = [" << systecCallReturn << "]";
		::UcanDeinitCanEx(canModuleHandle, m_channelNumber);
		return -1;
	}
	setCanHandle(m_moduleNumber, canModuleHandle);
	m_UcanHandle = canModuleHandle;
	return 0;
}

bool STCanScan::sendErrorCode(long status)
{
	char errorMessage[120];
	timeval ftTimeStamp; 
	if (status != m_busStatus) {
		gettimeofday(&ftTimeStamp,0);
		if (!errorCodeToString(status, errorMessage))
			canMessageError(status, errorMessage, ftTimeStamp);
		m_busStatus = status;
	}
	return true;
}

bool STCanScan::sendMessage(short cobID, unsigned char len, unsigned char *message, bool rtr)
{
	tCanMsgStruct canMsgToBeSent;
	BYTE Status;
	unsigned char *unprocessedMessageBuffer = message;
	canMsgToBeSent.m_dwID = cobID;
	canMsgToBeSent.m_bDLC = len;
	canMsgToBeSent.m_bFF = 0;
	if (rtr)
	{
		canMsgToBeSent.m_bFF = USBCAN_MSG_FF_RTR;
	}
	int unprocessedMessageLength, messageLengthToBeProcessed;
	unprocessedMessageLength = len;
	do
	{
		if (unprocessedMessageLength > 8)//If there is more than 8 characters to process, we process 8 of them in this iteration of the loop
		{
			messageLengthToBeProcessed = 8;
			unprocessedMessageLength = unprocessedMessageLength - 8;
		}
		else //Otherwise if there is less than 8 characters to process, we process all of them in this iteration of the loop
		{
			messageLengthToBeProcessed = unprocessedMessageLength;
		}
		canMsgToBeSent.m_bDLC = messageLengthToBeProcessed;
		memcpy(canMsgToBeSent.m_bData, unprocessedMessageBuffer, messageLengthToBeProcessed);
		MLOG(TRC,this) << "Channel Number: [" << m_channelNumber << "], cobID: [" << canMsgToBeSent.m_dwID << "], Message Length: [" << static_cast<int>(canMsgToBeSent.m_bDLC) << "]";
		Status = UcanWriteCanMsgEx(m_UcanHandle, m_channelNumber, &canMsgToBeSent, NULL);
		if (Status != USBCAN_SUCCESSFUL)
		{
			MLOG(ERR,this) << "Error: There was a problem when sending a message.";
			break;
		}
		else
		{
			m_statistics.onTransmit( canMsgToBeSent.m_bDLC );
		}
		unprocessedMessageBuffer = unprocessedMessageBuffer + messageLengthToBeProcessed;
	}
	while (unprocessedMessageLength > 8);
	return sendErrorCode(Status);
}

bool STCanScan::sendRemoteRequest(short cobID)
{
	tCanMsgStruct canMsg;
	BYTE Status;
	canMsg.m_dwID = cobID;
	canMsg.m_bDLC = 0;
	canMsg.m_bFF = USBCAN_MSG_FF_RTR;//Needed for send message rtr

	Status = UcanWriteCanMsgEx(m_UcanHandle, m_channelNumber, &canMsg, NULL);
	return sendErrorCode(Status);
}

bool STCanScan::errorCodeToString(long error, char message[])//TODO: Fix this method, this doesn't do anything!!
{
  char tmp[300];
// canGetErrorText((canStatus)error, tmp, sizeof(tmp));
// *message = new char[strlen(tmp)+1];	
    message[0] = 0;
// strcpy(*message,tmp);
  return true;
}

void STCanScan::getStatistics( CanStatistics & result )
{
	m_statistics.computeDerived (m_baudRate);
	result = m_statistics;  // copy whole structure
	m_statistics.beginNewRun();
}

bool STCanScan::initialiseLogging(LogItInstance* remoteInstance)
{
	LOG(Log::INF) << "DLL: logging initialised";
	bool ret = Log::initializeDllLogging(remoteInstance);
	LOG(Log::INF) << "DLL: logging initialised";
	return ret;
}
