/*
    Raw web server so that it is possible to generate Canvas code on the fly.

    Ted Rossin  5-10-2023
                7-20-2023

    ToDo:

   Things I've learned by trial and error and looking at the httpd application example:
    1. Do I need \r\n or just \n?  Just \n
    2. On Chrome, the first request is for the page (GET / HTTP/1.1).  
       The second request is for the icon (GET /favicon.ico)
    3. Not sure if the check for sndqueuelen >= TCP_SND_QUEUELEN is needed to skip the write.
    4. Just close the client and not the server after sending the entire page.
    5. Was able to increase MEM_SIZE to 20000 in lwipopts.h and it still worked
       When I tried before it would crash but I fixed some malloc free issues
       and all seems OK now.
 */

#include "cstring"
#include <cstdlib>
#include <cstdarg>
#include <regex>
#include "pico/stdlib.h"
#include "WebServer-lwip.h"
#include "HttpResponse.h"

#define TCP_PORT 80

static WebServer_t *mCurrentMsgState;
static WebServerLwip *LocalWs;

WebServerLwip::WebServerLwip()
{
    LocalWs = this;
    mCurrentMsgState = nullptr;
    mNumPending = 0;
}

WebServer_t *WebServerLwip::StateInit()
{
    auto *state = (WebServer_t *) calloc(1, sizeof(WebServer_t));
    if (!state)
    {
        //printf("failed to allocate state\n");
        return nullptr;
    }
    return state;
}

void WebServerLwip::CloseConnection(struct tcp_pcb *tpcb)
{
    if (tpcb != nullptr)
    {
        tcp_arg(tpcb, nullptr);
        tcp_sent(tpcb, nullptr);
        tcp_recv(tpcb, nullptr);
        tcp_err(tpcb, nullptr);
        tcp_close(tpcb);
#ifdef DEBUG_WEBSRV
        printf("CloseConnection: tcp_close of client was called\n");
#endif
    }
}

void WebServerLwip::CloseClientAndServer()
{
    int i;

    for (i = 0; i < mNumPending; i++)
    {
#ifdef DEBUG_WEBSRV
        printf("Pending Index=%d  Id=%d ClientClosed=%d client_pcb==NULL=%d\n", i, mPending[i]->Id,
               mPending[i]->ClientClosed, (mPending[i]->client_pcb == nullptr) ? 1 : 0);
#endif
        if (mPending[i]->client_pcb != nullptr)
        {
            tcp_arg(mPending[i]->client_pcb, nullptr);
            tcp_sent(mPending[i]->client_pcb, nullptr);
            tcp_recv(mPending[i]->client_pcb, nullptr);
            tcp_err(mPending[i]->client_pcb, nullptr);
            tcp_close(mPending[i]->client_pcb);
#ifdef DEBUG_WEBSRV
            printf("tcp_close of client was called. Id=%d\n", mPending[i]->Id);
#endif
        }
    }
    mNumPending = 0;

    if (mServerPcb)
    {
        tcp_arg(mServerPcb, nullptr);
        tcp_close(mServerPcb);
        mServerPcb = nullptr;
#ifdef DEBUG_WEBSRV
        printf("tcp_close of server was called\n");
#endif
    }
}

int WebServerLwip::SendMsg(PrivateWebMsg_t Msg)
{
    if (mMqInfo.Size == MSG_QUEUE_SIZE)
        return (-1); // Bummer, the event queue is full

    mMsgQueue[mMqInfo.Wr].Type = Msg.Type;
    mMsgQueue[mMqInfo.Wr].Id = Msg.Id;
    mMsgQueue[mMqInfo.Wr].Data = Msg.Data;
    mMsgQueue[mMqInfo.Wr].State = Msg.State;
    mMqInfo.Wr++;
    if (mMqInfo.Wr == MSG_QUEUE_SIZE)
        mMqInfo.Wr = 0;
    mMqInfo.Size++;
#ifdef DEBUG_WEBSRV
    printf("SendMsg: Type=%d Message Queue size is %d\n", Msg.Type, mMqInfo.Size);
#endif
    return (0);
}

WebMsg_t WebServerLwip::ReadMessage()
{
    WebMsg_t Msg;
    static int PreviousRequest = MSG_EMPTY;

    if (PreviousRequest == MSG_REQUEST)
    {
        // Entire page was sent
        if (mCurrentMsgState)
        {
            mCurrentMsgState->ClientClosed = 1;
            if (mCurrentMsgState->RBuf)
            {
                free(mCurrentMsgState->RBuf);
                mCurrentMsgState->RBuf = nullptr;
            }
        }
    }
    if (PreviousRequest == MSG_CANCELED)
    {
        if (mCurrentMsgState)
        {
            // This should have already happened
            if (mCurrentMsgState->RBuf)
            {
                free(mCurrentMsgState->RBuf);
                mCurrentMsgState->RBuf = nullptr;
            }
            // No need to close connection as it has already been closed.  Just clean up the mess
            free(mCurrentMsgState);
            mCurrentMsgState = nullptr;
        }
    }
    PreviousRequest = MSG_EMPTY;
    if (mMqInfo.Size == 0)
    {
        Msg.Type = MSG_EMPTY;
        Msg.Id = 0;
        Msg.Data = nullptr;
        return (Msg);
    }
    Msg.Type = mMsgQueue[mMqInfo.Rd].Type;
    PreviousRequest = Msg.Type;
    Msg.Type = mMsgQueue[mMqInfo.Rd].Type;
    Msg.Id = mMsgQueue[mMqInfo.Rd].Id;
    Msg.Data = mMsgQueue[mMqInfo.Rd].Data;
    mCurrentMsgState = mMsgQueue[mMqInfo.Rd].State;
    mMqInfo.Rd++;
    if (mMqInfo.Rd == MSG_QUEUE_SIZE)
        mMqInfo.Rd = 0;
    mMqInfo.Size--;
    return (Msg);
}

void WebServerLwip::AddConnectionToTracker(WebServer_t *State)
{
    if (mNumPending + 1 >= MAX_PENDING)
    {
#ifdef DEBUG_WEBSRV
        printf("AddConnectionToTracker: No more space to add Id=%d\n", State->Id);
#endif
        return;
    }
    mPending[mNumPending] = State;
    mNumPending++;
#ifdef DEBUG_WEBSRV
    printf("AddConnectionToTracker: Pending=%d.  Id=%d\n", mNumPending, State->Id);
#endif
}

void WebServerLwip::RemoveConnectionFromTracker(WebServer_t *State)
{
    int i, j;

    if (mNumPending == 0)
    {
#ifdef DEBUG_WEBSRV
        printf("RemoveConnectionFromTracker: Tracker is empty so Id=%d not found\n", State->Id);
#endif
        return;
    }
    for (i = 0; i < mNumPending; i++)
    {
        if (mPending[i] == State)
        {
            for (j = i; j < mNumPending - 1; j++)
                mPending[j] = mPending[j + 1];
            mNumPending--;
            mMessages.erase(State->Id);
#ifdef DEBUG_WEBSRV
            printf("RemoveConnectionFromTracker: Pending=%d\n", mNumPending);
#endif
            return;
        }
    }
#ifdef DEBUG_WEBSRV
    printf("RemoveConnectionFromTracker: Tracker is not empty but Id=%d not found\n", State->Id);
#endif
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
// This is called after some bytes have been sent.  The write returns before
// all is done.  This is called when client acks some/all data.
err_t WebServerLwip::TcpServerSent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    PrivateWebMsg_t Msg;
    auto *state = (WebServer_t *) arg;

    if (state == nullptr)
    {
#ifdef DEBUG_WEBSRV
        printf("TcpServerSent: Got call back with NULL state\n");
#endif
        return ERR_OK;
    }
    if (state->Canceled)
    {
#ifdef DEBUG_WEBSRV
        printf("TcpServerSent: Got call back with Canceled state  Id=%d\n", state->Id);
#endif
        return ERR_OK;
    }
    state->BytesPending -= len;
#ifdef DEBUG_WEBSRV
    printf("TcpServerSent: Ack of %u bytes. %u bytes outstanding.  Id=%d\n", len, state->BytesPending, state->Id);
#endif
    if (state->BytesPending == 0)
    {
        if (state->ClientClosed)
        {
            LocalWs->CloseConnection(tpcb);
            state->client_pcb = nullptr;
            if (state->Hidden == 0)
            {
                Msg.Type = MSG_CLOSED;
                Msg.Id = state->Id;
                Msg.Data = nullptr;
                Msg.State = state;
                if (LocalWs->SendMsg(Msg))
                {
#ifdef DEBUG_WEBSRV
                    printf("Message queue full\n");
#endif
                }
            }
#ifdef DEBUG_WEBSRV
            printf("TcpServerSent: Closed Id:%d and freed state\n", state->Id);
#endif
            LocalWs->RemoveConnectionFromTracker(state);

            free(state);
        }
    }
    else
    {
#ifdef DEBUG_WEBSRV
        printf("Still need ack of another %u bytes\n", state->BytesPending);
#endif
    }
    return ERR_OK;
}
#pragma clang diagnostic pop

#define CHUNK_SIZE  3000

int WebServerLwip::Printf(const char *Format, ...)
{
    int err;
    size_t Len;
    char *Data;
    va_list marker;

    // Guess how long the final string will be once parameters are expanded
    Len = strlen(Format) + 3000;
    Data = (char *) malloc(Len);
    if (Data == nullptr)
    {
        Data = (char *) malloc(CHUNK_SIZE);
        if (Data == nullptr)
            return (-1);
        Len = CHUNK_SIZE;
    }
    va_start(marker, Format);
    vsnprintf(Data, Len, Format, marker);

    err = SendText(Data);
    va_end(marker);
    free(Data);
    return (err);
}


int WebServerLwip::SendText(const char *Data)
{
    char TmpData[CHUNK_SIZE + 1];
    int Offset, Err;
    size_t Len;

    Len = strlen(Data);
    Offset = 0;
    if (Len < CHUNK_SIZE)
    {
        return (SendTheData(Data, Len));
    }
    // Need to break up into smaller pieces
    while (Offset < Len)
    {
        strncpy(TmpData, &Data[Offset], CHUNK_SIZE);
        TmpData[CHUNK_SIZE] = 0;
        Err = SendTheData(TmpData, strlen(TmpData));
        Offset += CHUNK_SIZE;
        if (Err)
            return (Err);
    }
    return (0);
}

int WebServerLwip::SendImage(const char *Type, int Size, const char *Data)
{
    Printf("HTTP/1.1 200 OK\n"
           "Content-Type: image/%s\n"
           "Content-Length: %d\n"
           "\n", Type, Size);
    return (Write(Data, Size));
}

int WebServerLwip::Write(const char *Data, int Size)
{
    int Offset, Len, Err;

    Offset = 0;
    if (Size < CHUNK_SIZE)
    {
        return (SendTheData(Data, Size));
    }
    // Need to break up into smaller pieces
    while (Offset < Size)
    {
        Len = (Size - Offset > CHUNK_SIZE) ? CHUNK_SIZE : Size - Offset;
        Err = SendTheData(&Data[Offset], Len);
        Offset += CHUNK_SIZE;
        if (Err)
            return (Err);
    }
    return (0);
}

int WebServerLwip::SendTheData(const char *Data, size_t Len)
{
    err_t err;

    if (mCurrentMsgState->Canceled)
    {
        return (-1);
    }

    // Do I need to xmit the extra NULL character? No need.

    mCurrentMsgState->BytesPending += Len;

    err = ERR_MEM;
    while (err == ERR_MEM)
    {
        err = tcp_write(mCurrentMsgState->client_pcb, Data, Len, TCP_WRITE_FLAG_COPY);
        if (err == ERR_MEM)
        {
            //printf("ERR_MEM: MaxSendLen=%d MaxSendQLen=%d TCP_SND_QUEUELEN=%d\n"
            //    ,MaxSendLen,MaxSendQLen,TCP_SND_QUEUELEN);  // QUEUELEN=32
            sleep_ms(1);
        }
    }
    if (err == ERR_CONN)
    {
        mCurrentMsgState->Canceled = 1;  // This may have already happened as this is just a 
        // race condition from recive getting a NULL and setting Canceled.
        return (-1);
    }
    if ((err != ERR_OK))
    {
#ifdef DEBUG_WEBSRV
        int MaxSendLen = tcp_sndbuf(mCurrentMsgState->client_pcb);   // 10K to 8K        
        printf("Failed to write %d bytes err=%d sndbuf=%d Qlen=%d\n", Len, err,
               MaxSendLen, tcp_sndqueuelen(mCurrentMsgState->client_pcb));
#endif
// REVISIT: Do something nicer.  
        mCurrentMsgState->Canceled = 1;
        return (-1);
    }
    tcp_output(mCurrentMsgState->client_pcb);
    return (0);
}

err_t WebServerLwip::SendNotFound(void *arg, struct tcp_pcb *tpcb)
{
    auto *state = (WebServer_t *) arg;
    char XmitBuf[100];

    if (arg == nullptr)
        return (ERR_OK);
    if (tpcb == nullptr)
        return (ERR_OK);

    sprintf(XmitBuf, "HTTP/1.1 404 Not Found\nConnection: close\n\n");

    size_t Len = strlen(XmitBuf);  // Don't need to send string terminator so no +1
    state->BytesPending = Len;
#ifdef DEBUG_WEBSRV
    printf("SendNotFound: Writing %d bytes to client.  Id=%d\n", Len, state->Id);
#endif

    err_t err = tcp_write(tpcb, XmitBuf, Len, TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK)
    {
        //printf("Failed to write data %d (ERR_MEM=%d)\n", err,err==ERR_MEM?1:0);
        return (err);
    }
    return ERR_OK;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantFunctionResult"
err_t WebServerLwip::TcpServerReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, __attribute((unused)) err_t err)
{
    PrivateWebMsg_t Msg;
    auto *state = (WebServer_t *) arg;

    if (state == nullptr)
    {
#ifdef DEBUG_WEBSRV
        printf("TcpServerReceive: arg was NULL\n");
#endif
        return ERR_OK;
    }
    state->ClientClosed = 0;
    //printf("TcpServerReceive:\n");
    if (!p)
    {
        LocalWs->CloseConnection(tpcb);
#ifdef DEBUG_WEBSRV
        printf("TcpServerReceive: Got null pointer for pbuf.  This means connection has been closed. Id=%d\n",
               state->Id);
#endif
        LocalWs->RemoveConnectionFromTracker(state);

        state->Canceled = 1;
        Msg.Type = MSG_CANCELED;
        Msg.Id = state->Id;
        Msg.Data = nullptr;
        state->RBuf = nullptr;
        Msg.State = state;
        if (LocalWs->SendMsg(Msg))
        {
#ifdef DEBUG_WEBSRV
            printf("TcpServerReceive: Message queue is full\n");
#endif
        }
//        free(state); // Message handler will free the state
        return ERR_OK;
    }
    if (p->tot_len > 0)
    {
        state->RBuf = (char *) malloc(p->tot_len + 1);
        if (state->RBuf == nullptr)
        {  // By default, this will never happen and the PICO will just throw an exception.
#ifdef DEBUG_WEBSRV
            printf("Out of memory for recieve buffer\n");
#endif
            tcp_recved(tpcb, p->tot_len);
            pbuf_free(p);
            return ERR_OK;
        }
        strncpy(state->RBuf, (char *) p->payload, p->tot_len);
        state->RBuf[p->tot_len] = 0;
#ifdef DEBUG_WEBSRV
        char Tmp[100], *Ptr;
        strncpy(Tmp, state->RBuf, 99);
        Tmp[99] = 0;
        Ptr = strstr(Tmp, "\r");
        if (Ptr)
            *Ptr = 0;
        printf("Id=%d: Got %d bytes: %s\n", state->Id, p->tot_len, Tmp);
#endif
        tcp_recved(tpcb, p->tot_len);
    }
    else
    {
        tcp_recved(tpcb, 0);
        state->RBuf[0] = 0;
#ifdef DEBUG_WEBSRV
        printf("Got zero bytes in TcpServerReceive. Id=%d\n", state->Id);
#endif
    }
    pbuf_free(p);

    if (!strncmp("GET /favicon.ico HTTP", state->RBuf, 21))
    {
        LocalWs->SendNotFound(arg, tpcb);
        if (state->RBuf)
        {
            free(state->RBuf);
            state->RBuf = nullptr;
        }
        state->Hidden = 1;
        state->RBuf = nullptr;
        state->ClientClosed = 1;
    }
    else
    {
        state->Hidden = 0;
        Msg.Type = MSG_REQUEST;
        Msg.Id = state->Id;
        Msg.Data = state->RBuf;
        Msg.State = state;
        if (LocalWs->SendMsg(Msg))
        {
#ifdef DEBUG_WEBSRV
            printf("TcpServerReceive2: Message queue is full\n");
#endif
        }
    }

    return ERR_OK;
}
#pragma clang diagnostic pop

void WebServerLwip::TcpServerError(void *arg, err_t err)
{
    // An ERR_RST is sent when the page load is canceled
#ifdef DEBUG_WEBSRV
    auto *state = (WebServer_t *) arg;
    printf("tcp_client_err_fn %d.  Id=%d\n", err, state->Id);
#endif
}

err_t WebServerLwip::TcpServerAccept(__attribute((unused)) void *arg, struct tcp_pcb *client_pcb, err_t err)
{
    static unsigned int Id = 0;

    if (err != ERR_OK || client_pcb == nullptr)
    {
#ifdef DEBUG_WEBSRV
        printf("TcpServerAccept err=%d client_pcbIsNULL=%d\n", err, client_pcb == nullptr ? 1 : 0);
#endif
        // No need to throw in the towel.  Just wait for another try.
        return (ERR_VAL);
    }
#ifdef DEBUG_WEBSRV
    printf("\n\nTcpServerAccept: Client connected. NewId=%d  Pending=%d\n", Id, LocalWs->mNumPending);
#endif
    WebServer_t *state = LocalWs->StateInit();
    if (!state)
        return (ERR_VAL);

    state->client_pcb = client_pcb;
    state->Id = Id++;
    LocalWs->AddConnectionToTracker(state);

    tcp_arg(client_pcb, state);
    tcp_sent(client_pcb, TcpServerSent);
    tcp_recv(client_pcb, TcpServerReceive);
    tcp_err(client_pcb, TcpServerError);
    return (ERR_OK);
}

int WebServerLwip::Init(int RetryForever)
{
    int Connected, Try = 0;

    do
    {
        Connected = (InitBase() ? 0 : 1);
        if (!Connected)
        {
            printf("Failed to open socket.  Try=%d\n", ++Try);
            sleep_ms(1000);
            CloseClientAndServer();
        }

    } while (!Connected && RetryForever);
    return (Connected ? 0 : -1);
}

int WebServerLwip::InitBase()
{
    mMqInfo.Rd = 0;
    mMqInfo.Wr = 0;
    mMqInfo.Size = 0;
    mCurrentMsgState = nullptr;

    mServerPcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!mServerPcb)
    {
#ifdef DEBUG_WEBSRV
        printf("Init: failed to create pcb\n");
#endif
        return (-1);
    }

    tcp_setprio(mServerPcb, 1);
    // If restarting a connection, it seems to take 113s for bind to work again.
    int Time = 0;
    err_t err;
    do
    {
        err = tcp_bind(mServerPcb, nullptr, TCP_PORT);
        if (err)
        {
            // Need SO_REUSE=1 in lwipopts.h but it does not work.
#ifdef DEBUG_WEBSRV
            printf("Init: failed to bind to port %u: ERR_USE=%d ERR_VAL=%d Retrying: %d seconds\n", TCP_PORT,
                   err == ERR_USE ? 1 : 0, err == ERR_VAL ? 1 : 0, Time);
#endif
            Time++;
            sleep_ms(1000);
        }
    } while (err && (Time < 60 * 5));
    if (err)
    {
#ifdef DEBUG_WEBSRV
        printf("Init: failed to bind to port %u: ERR_USE=%d ERR_VAL=%d %d seconds\n", TCP_PORT, err == ERR_USE ? 1 : 0,
               err == ERR_VAL ? 1 : 0, Time);
#endif
        return (-1);
    }
    mServerPcb = tcp_listen(mServerPcb);
    if (!mServerPcb)
    {
#ifdef DEBUG_WEBSRV
        printf("\nInit: failed to listen\n");
#endif
        if (mServerPcb)
            tcp_close(mServerPcb);
        return (-1);
    }

    printf("Socket open\n");
    tcp_arg(mServerPcb, nullptr);
    tcp_accept(mServerPcb, TcpServerAccept);
    return (0);
}

void WebServerLwip::ProcessMessages(HttpResponse (*Cb)(const char *Request))
{
    WebMsg_t Msg;

    do
    {
        Msg = ReadMessage();
        switch (Msg.Type)
        {
        case MSG_REQUEST:
        {
            if (mMessages.count(Msg.Id))
                mMessages[Msg.Id].append(Msg.Data);
            else
                mMessages[Msg.Id] = Msg.Data;

            if (IsValidRequest(mMessages[Msg.Id]))
            {
                HttpResponse response = (*Cb)(mMessages[Msg.Id].c_str());
                WebServerLwip::Printf(response.ToString().c_str());
                mMessages.erase(Msg.Id);
            }
            break;
        }
        case MSG_CLOSED:
        case MSG_CANCELED:
            break;
        }
    } while (Msg.Type != MSG_EMPTY);
}

bool WebServerLwip::IsValidRequest(std::string req)
{
    req = std::regex_replace(req, std::regex("\r"), ""); // Remove CR
    if (req.find("\n\n") == std::string::npos)
        return false;
    auto head = req.substr(0, req.find("\n\n")); // Find first double LF
    auto m = head.substr(0, head.find('\n'));
    std::string headers;
    headers = head.substr(m.length() + 1);

    std::list<std::pair<std::string, std::string>> headerList;

    std::istringstream h(headers);
    std::string line;

    while (std::getline(h, line))
    {
        if (line.find(':') != -1)
        {
            auto name = line.substr(0, line.find(':'));
            auto value = line.substr(name.length() + 1);

            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            regex_replace(value, std::regex("^ *"), "");
            headerList.emplace_back(name, value);
        }
    }

    auto contentLengthStr = std::find_if(headerList.begin(), headerList.end(), [](auto el){return el.first == "content-length";});
    int contentLength = 0;
    if (contentLengthStr != headerList.end())
    {
        contentLength = std::stoi((*contentLengthStr).second);
    }

    if (req.length() < head.length() + 2 + contentLength)
        return false;

    return true;
}
