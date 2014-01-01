

// ref1 : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365601(v=vs.85).aspx
// ref2 : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365592(v=vs.85).aspx

#include "logger.h"
#include "cmdutil.h"

HANDLE CmdUtil::_pipe = NULL;
bool CmdUtil::_inited = false;
CmdUtil* CmdUtil::_instance = NULL;
DWORD CmdUtil::_result = 0;
DWORD CmdUtil::_timeout = 1000;
TCHAR CmdUtil::_pipename[256] = {0};

CmdUtil& CmdUtil::GetInst()
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

NEXT_STEP CmdUtil::ParseCmd(LPCTSTR pipename)
{
    LPWSTR *szArglist;
    int nArgs;
    NEXT_STEP result = NS_NONE;

    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (NULL == szArglist)
    {
        LOG("error(%d): parse-cmd fail.", GetLastError());
        result = NS_EXIT;
    }
    else if (1 == nArgs)
    {
        LOG("nArgs = %d", nArgs);
        /*��������������ж��Ƿ���ʵ���Ѿ�������*/
        HANDLE pipe = OpenPipe(pipename);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            result = NS_CONTINUE;
        }
        else
        {
            result = NS_EXIT;
            ClosePipe();
        }
    }
    else if (2 == nArgs && 0 == lstrcmpi(szArglist[1], _T("-exit")))
    {
        HANDLE pipe = OpenPipe(pipename);
        if (pipe != INVALID_HANDLE_VALUE)
        {
            SendCmdRequest(szArglist[1]);
            Logger::Instance().Close();
            ClosePipe();
        }
        result = NS_EXIT;
    }
    else
    {
        result = NS_EXIT;
    }
    LocalFree(szArglist);
    return result;
}


BOOL CmdUtil::StartListenThread(LPCTSTR pipename)
{
    DWORD dwError = 0;
    HANDLE hThread = NULL;
    if (0 != _tcscpy_s(_pipename, _countof(_pipename), pipename))
    {
        LOG("error(%d): _tcscpy_s() fail.", errno);
        return FALSE;
    }
    hThread = CreateThread(NULL, 0, ListenCmdProc, NULL, 0, NULL);
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
        LOG("ok: create connect event(handle = 0x%p).", hConnectEvent);
    }

    OVERLAPPED oConnect;
    oConnect.hEvent = hConnectEvent;
    BOOL fPendingIO = CreateAndConnectInstance(&oConnect);

    while (true)
    {
        /*
        ���пͻ�������ʱ���߶�д�������wait�������� 
        */
        LOG("waiting for event (hEvent = 0x%p)...", hConnectEvent);
        DWORD dwWait = WaitForSingleObjectEx(hConnectEvent, INFINITE, TRUE);
        LOG("catch event: dwWait = %d", dwWait);

        switch (dwWait)
        {
        case 0:
            {
                if (fPendingIO) 
                {
                    DWORD cbRet; 
                    BOOL fSuccess = GetOverlappedResult(
                        _pipe, &oConnect, &cbRet, FALSE);

                    LOG("fSuccess = %d, _pipe = 0x%p", fSuccess, _pipe);
                    if (!fSuccess) 
                    {
                        LOG("error(%d): ConnectNamedPipe", GetLastError());
                        return 0;
                    }
                }

                /*Ϊ�ܵ�ʵ������洢�ռ�*/
                lpPipeInst = (LPPIPEINST) GlobalAlloc( GPTR, sizeof(PIPEINST));
                LOG("lpPipeInst = 0x%p", lpPipeInst);
                if (lpPipeInst == NULL) 
                {
                    LOG("error(%d): GlobalAlloc() failed", GetLastError()); 
                    return 0;
                }

                /*
                ԭ�еĹܵ����_pipe������lpPipeInst�������������ʹ�á�
                LPPIPEINST�ṹ�ĵ�һ����OVERLAPPED�����Կɶ�lpPipeInst����ǿת��
                ֮��client����������ͨ����ɺ����໥���ô������õ�_pipe��
                */
                lpPipeInst->hPipeInst = _pipe;
                lpPipeInst->cbToWrite = 0;
                CompletedWriteRoutine(0, 0, (LPOVERLAPPED) lpPipeInst);

                /*����һ���µĹܵ�ʵ���Դ�����һ���ͻ��˵���������*/
                fPendingIO = CreateAndConnectInstance(&oConnect);
                LOG("fPendingIO = %d", fPendingIO); 
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
        PIPE_BUFSIZE * sizeof(TCHAR),
        PIPE_BUFSIZE * sizeof(TCHAR),
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
            LOG("SetEvent");
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
    LOG("pipe = 0x%p", lpPipeInst->hPipeInst);
    if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
    {
        fRead = ReadFileEx(
            lpPipeInst->hPipeInst,
            lpPipeInst->chRequest,
            PIPE_BUFSIZE * sizeof(TCHAR),
            (LPOVERLAPPED) lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);
    }
    if (!fRead)
    {
        LOG("error(%d): ReadFileEx() fail.", GetLastError());
        DisconnectAndClose(lpPipeInst);
    }
    else
    {
        /*����ReadFileEx���سɹ�Ҳ��Ҫ�鿴�Ƿ��б����Ҫע�����Ϣ*/
        DWORD lastErr = GetLastError();
        if (ERROR_SUCCESS != lastErr && ERROR_IO_PENDING != lastErr)
        {
            LOG("ReadFileEx() success but GLE = %d", GetLastError());
        }
    }
}


VOID  CmdUtil::CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
                                   LPOVERLAPPED lpOverLap)
{
    LOG("dwErr = %d, cbBytesRead = %d", dwErr, cbBytesRead);
    BOOL fWrite = FALSE;
    LPPIPEINST lpPipeInst = (LPPIPEINST) lpOverLap;
    LOG("pipe = 0x%p", lpPipeInst->hPipeInst);

    if ((dwErr == 0) && (cbBytesRead != 0))
    {
        LOGT(_T("request = %s"), lpPipeInst->chRequest);
        if (0 == _tcscmp(_T("-exit"), lpPipeInst->chRequest))
        {
            LOG("guard exit: [ok]");
            exit(0);
        }

        *((DWORD *)(lpPipeInst->chReply)) = 0x2104;
        lpPipeInst->cbToWrite = sizeof(DWORD);

        fWrite = WriteFileEx(
            lpPipeInst->hPipeInst,
            &(lpPipeInst->chReply),
            lpPipeInst->cbToWrite,
            (LPOVERLAPPED) lpPipeInst,
            (LPOVERLAPPED_COMPLETION_ROUTINE) CompletedWriteRoutine); 
    }
    if (! fWrite)
    {
        LOG("error(%d): WriteFileEx() fail.", GetLastError());
        DisconnectAndClose(lpPipeInst);
    }
    else
    {
        /*����WriteFileEx���سɹ�Ҳ��Ҫ�鿴�Ƿ��б����Ҫע�����Ϣ*/
        DWORD lastErr = GetLastError();
        if (ERROR_SUCCESS != lastErr && ERROR_IO_PENDING != lastErr)
        {
            LOG("WriteFileEx() success but GLE = %d", GetLastError());
        }
    }
}

/*
����������߿ͻ��˹رչܵ�ʱ����
*/
VOID CmdUtil::DisconnectAndClose(LPPIPEINST lpPipeInst)
{
    LOG("lpPipeInst = 0x%p", lpPipeInst);
    if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
    {
        LOG("error(%d): DisconnectNamedPipe failed", GetLastError());
    }
    if (NULL != lpPipeInst)
    {
        if (!CloseHandle(lpPipeInst->hPipeInst))
        {
            LOG("error(%d): close handle fail.", GetLastError());
        }
        GlobalFree(lpPipeInst);
    }
}


void CmdUtil::Release()
{
    delete this;
    _instance = NULL;
    _inited = false;
}


/************************************************************************/
/* �ͻ�������
/************************************************************************/

HANDLE CmdUtil::OpenPipe(LPCTSTR pipename)
{
    _pipe = CreateFile(pipename, GENERIC_READ | GENERIC_WRITE, 
        0, NULL, OPEN_EXISTING, 0, NULL); 
    return _pipe;
}

BOOL CmdUtil::SendCmdRequest(LPCTSTR cmd)
{
    DWORD  cbRead, cbToWrite, cbWritten, dwMode; 
    dwMode = PIPE_READMODE_MESSAGE; 
    TCHAR recv[PIPE_BUFSIZE];
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
    do
    {
        fSuccess = ReadFile(_pipe, recv, PIPE_BUFSIZE * sizeof(TCHAR),
            &cbRead, NULL);
        if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
            break;
        LOG("cbRead = %d, responce: 0x%x", cbRead, *(DWORD*)recv);
    } while ( !fSuccess);

    return TRUE;
}


BOOL CmdUtil::ClosePipe(void)
{
    if (!CloseHandle(_pipe))
    {
        LOG("error(%d): CloseHandle() failed", GetLastError());
        return FALSE;
    }
    return TRUE;
}