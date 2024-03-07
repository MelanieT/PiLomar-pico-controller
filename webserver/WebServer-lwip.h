#ifndef WEB_SERVER_PICO_H
#define WEB_SERVER_PICO_H

#include <map>
#include "WebServer.h"

#include "lwip/tcp.h"

#define MSG_EMPTY       0
#define MSG_REQUEST     1
#define MSG_CLOSED      2
#define MSG_CANCELED    3

#define INIT_RETRY_FOREVER  1
#define INIT_SINGLE_TIMEOUT 0

typedef struct
{
    unsigned int Id;
    int Type;
    const char *Data;
} WebMsg_t;

typedef struct
{
    struct tcp_pcb *client_pcb;
    unsigned int BytesPending;
    char *RBuf;
    unsigned int Id;
    uint8_t Hidden;
    uint8_t ClientClosed;
    uint8_t Canceled;
} WebServer_t;

typedef struct
{
    unsigned int Id;
    int Type;
    char *Data;
    WebServer_t *State;
} PrivateWebMsg_t;

class WebServerLwip : public WebServer
{
public:
    WebServerLwip();

    int Init(int RetryForever = 1) override;
    void ProcessMessages(HttpResponse (*Cb)(const char *Request)) override;

private:
    WebMsg_t ReadMessage();
    static int Printf(const char *Format, ...);
    static int SendText(const char *Data);
    static int SendImage(const char *Type, int Size, const char *Data);
    static int Write(const char *Data, int Size);
    int InitBase();
    static WebServer_t *StateInit();
    static int SendTheData(const char *Data, size_t Len);
    static err_t SendNotFound(void *arg, struct tcp_pcb *tpcb);
    int SendMsg(PrivateWebMsg_t Msg);
    static void CloseConnection(struct tcp_pcb *tpcb);
    void CloseClientAndServer();
    static bool IsValidRequest(std::string req);
    // Note that call back functions have to be static
    static err_t TcpServerAccept(void *arg, struct tcp_pcb *client_pcb, err_t err);
    static void TcpServerError(void *arg, err_t err);
    static err_t TcpServerReceive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

    static err_t TcpServerSent(void *arg, struct tcp_pcb *tpcb, u16_t len);

    struct tcp_pcb *mServerPcb = nullptr;
    struct
    {
        uint8_t Rd, Wr, Size;
    } mMqInfo {0, 0, 0};
#define MSG_QUEUE_SIZE  20
    PrivateWebMsg_t mMsgQueue[MSG_QUEUE_SIZE] = {};

    void AddConnectionToTracker(WebServer_t *State);
    void RemoveConnectionFromTracker(WebServer_t *State);
    // Stupid array to deal with pending requests when WiFi is lost or the connection is reset.
    // This allows client_pcbs to be freed up that might never be acked or sent.
#define MAX_PENDING 50
    WebServer_t *mPending[MAX_PENDING] = {};
    int mNumPending;

    std::map<unsigned , std::string> mMessages = {};
};

#endif
