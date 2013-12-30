

// ref1 : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365601(v=vs.85).aspx
// ref2 : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365592(v=vs.85).aspx

#include "logger.h"
#include "cmdutil.h"


HANDLE CmdUtil::_pipe = NULL;
bool CmdUtil::_inited = false;
CmdUtil* CmdUtil::_instance = NULL;
DWORD CmdUtil::_result  = 0;
DWORD CmdUtil::_bufsize = 16;
DWORD CmdUtil::_timeout = 1000;
LPCTSTR CmdUtil::_pipename = _T("\\\\.\\pipe\\namedpipe_cmdutil");

CmdUtil& CmdUtil::Instance()
{
    if (!_inited)
    {
        _instance = new CmdUtil();
        _inited = true;
    }
    return *_instance;
}

/************************************************************************/
/* �����Ƿ���˹��ܴ���                                               */
/************************************************************************/

BOOL CmdUtil::StartListenThread()
{
    DWORD dwError = 0;
    HANDLE hThread = CreateThread(NULL, 0, ListenCmdProc, NULL, 0, NULL);
    if (NULL == hThread)
    {
        dwError = GetLastError();
        LOG("error: create listen-cmd-thread failed.");
        return FALSE;
    }
	return TRUE;
}


DWORD CmdUtil::ListenCmdProc(LPVOID lpParam)
{
    LPPIPEINST lpPipeInst;

    /*����һ���¼�����֪ͨ*/
    HANDLE hConnectEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (NULL == hConnectEvent)
    {
        LOG("error(%d): create connect-event fail.", GetLastError()); 
        return 0;
    }
	else
	{
		LOG("ok: create connect-event."); 
	}

    OVERLAPPED oConnect;
    oConnect.hEvent = hConnectEvent;
    BOOL fPendingIO = CreateAndConnectInstance(&oConnect);

    while (true)
    {
        /*
        ���пͻ�������ʱ���߶�д�������wait�������� 
        */
		LOG("waiting for event...");
        DWORD dwWait = WaitForSingleObjectEx(hConnectEvent, INFINITE, TRUE);
		LOG("dwWait = %d", dwWait); 
        switch (dwWait)
        {
        case 0:
            {
                if (fPendingIO) 
                {
                    DWORD cbRet; 
                    BOOL fSuccess = GetOverlappedResult(
						_pipe, &oConnect, &cbRet, FALSE);

					LOG("fSuccess = %d", fSuccess); 
                    if (!fSuccess) 
                    {
                        LOG("error(%d): ConnectNamedPipe", GetLastError()); 
                        return 0;
                    }
                } 

                /*Ϊ�ܵ�ʵ������洢�ռ�*/
                lpPipeInst = (LPPIPEINST) GlobalAlloc( GPTR, sizeof(PIPEINST)); 
                if (lpPipeInst == NULL) 
                {
                    LOG("error(%d): GlobalAlloc() failed", GetLastError()); 
                    return 0;
                }

                lpPipeInst->hPipeInst = _pipe;
                lpPipeInst->cbToWrite = 0;
                CompletedWriteRoutine(0, 0, (LPOVERLAPPED) lpPipeInst);

                /* Ϊ��һ�����Ӵ����µĹܵ�ʵ�� */
                fPendingIO = CreateAndConnectInstance( &oConnect);
                break;
            }

        case WAIT_IO_COMPLETION:
            {
                LOG("WAIT_IO_COMPLETION, break;"); 
                break;
            }
        default:
            {
                LOG("error(%d): WaitForSingleObjectEx() failed.", GetLastError()); 
                return 0;
            }
        }
    }
    return 0;
}


/*
����˽��̴��������ܵ����������ӵ��ܵ���
����ͻ���û�����ӵ��ܵ��򷵻سɹ���
����ͻ����Ѿ������ܵ����򷵻�ʧ�ܡ�
*/
BOOL CmdUtil::CreateAndConnectInstance(LPOVERLAPPED lpoOverlap)
{
    _pipe = CreateNamedPipe( 
        _pipename,
        PIPE_ACCESS_DUPLEX |FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        _bufsize,
        _bufsize,
        _timeout,
        NULL);

    if (_pipe == INVALID_HANDLE_VALUE) 
    {
        LOG("error(%d):CreateNamedPipe() failed", GetLastError()); 
        return FALSE;
    }
	else
	{
		LOG("ok: CreateNamedPipe().");
		LOG("**** _pipe = 0x%p ****", _pipe);
		return ConnectPipe(_pipe, lpoOverlap);
	}
}


BOOL CmdUtil::ConnectPipe(HANDLE hPipe, LPOVERLAPPED lpo)
{
    /*
    ����ConnectNamedPipe����ȴ��ͻ��˽������ӵ�״̬��
    ��Ϊ���첽��������ConnectNamedPipeӦ����������0
    ����GetLastError() = ERROR_IO_PENDING.
    */
    BOOL fConnected = ConnectNamedPipe(hPipe, lpo);
    LOG("info: ConnectNamedPipe() returns: %d.", fConnected);

    if (fConnected)
    {
        LOG("error(%d): ConnectNamedPipe failed", GetLastError()); 
        return 0;
    }

    BOOL fPendingIO = FALSE;
    switch (GetLastError()) 
    {
    case ERROR_IO_PENDING: 
        {
            fPendingIO = TRUE; 
            break;
        }
    case ERROR_PIPE_CONNECTED:
        {
            if (SetEvent(lpo->hEvent)) 
                break;
        }
    default:
        {
            LOG("error(%d): ConnectNamedPipe failed", GetLastError());
            return 0;
        }
    }
    return fPendingIO;
}


/*
* ��д�������֮����ߵ��ͻ����������ӵ��ܵ�ʱ���ñ�����
*/
VOID CmdUtil::CompletedWriteRoutine(DWORD dwErr, DWORD cbWritten, 
                                           LPOVERLAPPED lpOverLap) 
{
	LOG("dwErr = %d, cdWritten = %d", dwErr, cbWritten);
    BOOL fRead = FALSE;
    LPPIPEINST lpPipeInst = (LPPIPEINST) lpOverLap; 
    if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))  
    {
        fRead = ReadFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chRequest,
            _bufsize *sizeof(TCHAR),
            (LPOVERLAPPED) lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE) CompletedReadRoutine); 
    }
	LOG("ReadFileEx() returns %d", fRead);
    if (!fRead)
    {
        DisconnectAndClose(lpPipeInst);
    }
}


VOID CmdUtil::CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
                                          LPOVERLAPPED lpOverLap)
{
    BOOL fWrite = FALSE;
    LPPIPEINST lpPipeInst = (LPPIPEINST) lpOverLap; 
    if ((dwErr == 0) && (cbBytesRead != 0)) 
    {
		*((DWORD *)(lpPipeInst->chReply)) = 0;
		lpPipeInst->cbToWrite = sizeof(DWORD);

        fWrite = WriteFileEx(
            lpPipeInst->hPipeInst, 
            &(lpPipeInst->chReply), 
            lpPipeInst->cbToWrite,
            (LPOVERLAPPED) lpPipeInst, 
            (LPOVERLAPPED_COMPLETION_ROUTINE) CompletedWriteRoutine); 
    }
	LOG("WriteFileEx() returns %d", fWrite);
    if (! fWrite)
    {         
        DisconnectAndClose(lpPipeInst);  
    }
}

/*
����������߿ͻ��˹رչܵ�ʱ����
*/
VOID CmdUtil::DisconnectAndClose(LPPIPEINST lpPipeInst)
{
    if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
    {
        LOG("error(%d): DisconnectNamedPipe failed", GetLastError());
    }
    CloseHandle(lpPipeInst->hPipeInst);
    if (lpPipeInst != NULL)
    {
        GlobalFree(lpPipeInst); 
    }
}


void CmdUtil::Release()
{
    LOG(">>");
    delete this;
    _instance = NULL;
    _inited = false;
}


/************************************************************************/
/* �ͻ�������
/************************************************************************/

HANDLE CmdUtil::OpenPipe()
{
	_pipe = CreateFile(_pipename, GENERIC_READ | GENERIC_WRITE, 
		0, NULL, OPEN_EXISTING, 0, NULL); 
	return _pipe;
}

BOOL CmdUtil::SendCmd(LPCTSTR cmd)
{
	DWORD  cbRead, cbToWrite, cbWritten, dwMode; 
	dwMode = PIPE_READMODE_MESSAGE; 
	BOOL fSuccess = SetNamedPipeHandleState(_pipe, &dwMode, NULL, NULL);  
	if ( !fSuccess) 
	{
		LOG("error(%d): SetNamedPipeHandleState() failed", GetLastError());
		return FALSE;
	}

	/*��ܵ��ķ���˷������� */
	cbToWrite = (lstrlen(cmd) + 1) * sizeof(TCHAR);
	LOGT(_T("Sending %d byte message: [%s]"), cbToWrite, cmd); 

	fSuccess = WriteFile(_pipe, cmd, cbToWrite, &cbWritten, NULL);
	if ( !fSuccess) 
	{ 
		LOG("WriteFile to pipe failed. GLE = %d", GetLastError()); 
		return FALSE;
	}
	LOG("send cmd done");
	return TRUE;
}

BOOL CmdUtil::ClosePipe(void)
{
	return CloseHandle(_pipe); 
}

