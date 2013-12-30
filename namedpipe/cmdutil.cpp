
#include "stdafx.h"
#include "cmdutil.h"
#include "logger.h"

HANDLE CmdUtil::_pipe = NULL;
bool CmdUtil::_inited = false;
CmdUtil* CmdUtil::_instance = NULL;
DWORD CmdUtil::_result  = 0;
DWORD CmdUtil::_bufsize = 16;
DWORD CmdUtil::_timeout = 1000;
LPCTSTR CmdUtil::_piplename = _T("\\\\.\\pipe\\namedpipe_cmdutil");

CmdUtil* CmdUtil::GetInst()
{
    if (!_inited)
    {
        _instance = new CmdUtil();
        _inited = true;
    }
    return _instance;
}

/************************************************************************/
/* �����Ƿ���˹��ܴ���                                               */
/************************************************************************/

BOOL CmdUtil::StartListenThread()
{
    LOG(">>");
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
    LOG(">>");
    LPPIPEINST lpPipeInst;

    /*����һ���¼�����֪ͨ*/
    HANDLE hConnectEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
    if (NULL == hConnectEvent)
    {
        LOG("error(%d): create connect-event fail.", GetLastError()); 
        return 0;
    }

    OVERLAPPED oConnect;
    oConnect.hEvent = hConnectEvent;
    BOOL fPendingIO = CreateAndConnectInstance(&oConnect);

    while (true)
    {
        /*
        �ȴ��ͻ��˽��̵������¼����߶�д����¼� 
        */
        DWORD dwWait = WaitForSingleObjectEx(hConnectEvent, INFINITE, TRUE);
        switch (dwWait)
        {
        case 0:
            {
                if (fPendingIO) 
                {
                    DWORD cbRet; 
                    BOOL fSuccess = GetOverlappedResult(
                        _pipe,
                        &oConnect,
                        &cbRet,
                        FALSE);
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
    LOG(">>");
    _pipe = CreateNamedPipe( 
        _piplename,
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
    return ConnectPipe(_pipe, lpoOverlap);
}


BOOL CmdUtil::ConnectPipe(HANDLE hPipe, LPOVERLAPPED lpo)
{
    LOG(">>");
    /*
    ����ConnectNamedPipe����ȴ��ͻ��˽������ӵ�״̬��
    ��Ϊ���첽��������ConnectNamedPipeӦ����������0
    ����GetLastError() = ERROR_IO_PENDING.
    */
    BOOL fConnected = ConnectNamedPipe(hPipe, lpo);
    LOG("info: ConnectNamedPipe returns: %d.", fConnected);

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
    LOG(">>");
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

    if (! fRead)
    {
        DisconnectAndClose(lpPipeInst);
    }
}

// This routine is called as an I/O completion routine
// after reading a request from the client. 
// It gets data and writes it to the pipe. 
VOID CmdUtil::CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
                                          LPOVERLAPPED lpOverLap)
{
    LOG(">>");
    BOOL fWrite = FALSE;
    // lpOverlap points to storage for this instance. 
    LPPIPEINST lpPipeInst = (LPPIPEINST) lpOverLap; 

    //The read operation has finished, 
    //so write a response (if no error occurred). 
    if ((dwErr == 0) && (cbBytesRead != 0)) 
    { 
        //�Լ��Ѿ����꣬ҲӦ�ô�����ϣ�����Ҫ���ش�������

        //���ز����Ľ����Ҳ����������Ĵ���
        //��ȡ�����з��������ݣ�������ͷ��ؽṹ��

        //�涨���еĴ����������õ�һ��ȫ�ֱ�������Ȼ�󷵻����ֵ�� 

        //�ص��Ǹ�д���������
        //GetAnswerToRequest(lpPipeInst); 

        fWrite = WriteFileEx(
            lpPipeInst->hPipeInst, 
            &(lpPipeInst->chReply), 
            lpPipeInst->cbToWrite,
            (LPOVERLAPPED) lpPipeInst, 
            (LPOVERLAPPED_COMPLETION_ROUTINE) CompletedWriteRoutine); 
    }
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
    LOG(">>");
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
