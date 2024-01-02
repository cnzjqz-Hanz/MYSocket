#include <iostream>
#include <iomanip>
#include <string>
#include <WinSock2.h>
#include <Windows.h>
#include <chrono>
#include <ctime>
#include <map>
#include <vector>
#pragma comment(lib, "ws2_32.lib")
#define SPORT 13251
#define SIP "127.0.0.1"

using namespace std;

enum RQ_TYPE
{
    CHECK = 0x1,
    _ERROR,
    TIME,
    NAME,
    CLIENT_LIST,
    MESSAGE,
    CLOSE,
    EXIT
};

struct message
{
    RQ_TYPE type;
    char data[1024] = "";
};

class sClientInfo
{
private:
    short family;
    unsigned short port;
    uint32_t addr;

public:
    sClientInfo(short _f, unsigned short _p, uint32_t _a) : family(_f), port(_p), addr(_a) {}
    sClientInfo() {}
    short getFamily() { return family; }
    unsigned short getPort() { return port; }
    uint32_t getAddr() { return addr; }
};

map<SOCKET, sClientInfo> sClients;
HANDLE mutex;
SOCKET srvSock;

int sendString(SOCKET sClient, const char str[], int type = CHECK)
{
    message pack;
    pack.type = static_cast<RQ_TYPE>(type);
    memcpy(pack.data, str, 1024);
    return send(sClient, (char *)&pack, 1024, 0);
}

DWORD WINAPI ThreadProc(LPVOID lp)
{
    auto sClient = *(SOCKET *)lp;
    char sClientIPv4[4] = {0};
    uint32_t sClientAddr = 0;
    sClientAddr = sClients[sClient].getAddr();
    memcpy(sClientIPv4, &sClientAddr, 4);

    /* 返回给客户端的信息 */
    sendString(sClient, "Server connected.");

    /* 服务器连接成功提示 */
    cout << "-----------------------------\n";
    cout << "Connection from " << sClient << endl;
    cout << "Client IP: ";
    for (int i = 0; i < 4; i++)
    {
        if (i)
            cout << ".";
        cout << (int)sClientIPv4[i];
    }
    cout << ";\tPort: " << sClients[sClient].getPort() << endl;
    chrono::system_clock::time_point now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);
    cout << "Connection time: " << ctime(&currentTime) << endl;
    cout << "-----------------------------\n";

    sendString(sClient, "Server: Welcome to the server.");
    while (true)
    {
        message recvPack;
        if (recv(sClient, (char *)&recvPack, sizeof(recvPack), 0) != SOCKET_ERROR)
        {
            WaitForSingleObject(mutex, INFINITE);
            switch (recvPack.type)
            {
            case TIME:
            {
                /* 获取系统时间 */
                chrono::system_clock::time_point _t = chrono::system_clock::now();
                time_t currentTime = chrono::system_clock::to_time_t(_t);
                string timeStr = ctime(&currentTime);
                message pack;
                pack.type = TIME;
                memcpy(pack.data, timeStr.c_str(), 1024);
                cout << "[INFO] Send current time to " << sClient << endl;
                if (send(sClient, (char *)&pack, sizeof(pack), 0) != SOCKET_ERROR)
                    cout << "[INFO] Success!\n\n";
                else
                    cout << "[ERROR] Failed!\n\n";
                break;
            }
            case NAME:
            {
                /* 获取主机名 */
                char name[1024] = "";
                if (gethostname(name, 1024) == SOCKET_ERROR)
                {
                    sendString(sClient, "Server: Failed to get host name.");
                    break;
                }
                message pack;
                pack.type = NAME;
                memcpy(pack.data, name, 1024);
                cout << "[INFO] Send host name to " << sClient << endl;
                if (send(sClient, (char *)&pack, sizeof(pack), 0) != SOCKET_ERROR)
                    cout << "[INFO] Success!\n\n";
                else
                    cout << "[ERROR] Failed!\n\n";
                break;
            }
            case CLIENT_LIST:
            {
                /* 获取客户端列表 */
                string clientList = "";
                for (auto it = sClients.begin(); it != sClients.end(); it++)
                {
                    char sClientIPv4[4] = {0};
                    uint32_t sClientAddr = 0;
                    sClientAddr = it->second.getAddr();
                    memcpy(sClientIPv4, &sClientAddr, 4);
                    clientList += to_string(it->first) + "\t";
                    for (int i = 0; i < 4; i++)
                    {
                        if (i)
                            clientList += ".";
                        clientList += to_string((int)sClientIPv4[i]);
                    }
                    clientList += "\t" + to_string(it->second.getPort()) + "\n";
                }
                message pack;
                pack.type = CLIENT_LIST;
                memcpy(pack.data, clientList.c_str(), 1024);
                cout << "[INFO] Send client list to " << sClient << endl;
                if (send(sClient, (char *)&pack, sizeof(pack), 0) != SOCKET_ERROR)
                    cout << "[INFO] Success!\n\n";
                else
                    cout << "[ERROR] Failed!\n\n";
                break;
            }
            case MESSAGE:
            {
                // 消息格式：IP:PORT MESSAGE
                string msg = recvPack.data;
                int pos = msg.find(":");
                if (pos == string::npos)
                {
                    sendString(sClient, "[SERVER] Invalid format.", _ERROR);
                    break;
                }
                string addr = msg.substr(0, pos);
                pos = msg.find(" ");
                if (pos == string::npos)
                {
                    sendString(sClient, "[SERVER] Invalid format.", _ERROR);
                    break;
                }
                string port = msg.substr(addr.length() + 1, msg.find(" ") - msg.find(":") - 1);
                int portInt = stoi(port);
                if (portInt < 0 || portInt > 65535)
                {
                    sendString(sClient, "[SERVER] Invalid port.", _ERROR);
                    break;
                }
                string content = msg.substr(msg.find(" ") + 1);
                auto addrInt = inet_addr(addr.c_str());
                if (addrInt == INADDR_NONE)
                {
                    sendString(sClient, "[SERVER] Invalid IP.", _ERROR);
                    break;
                }
                bool flag = false;
                for (auto it = sClients.begin(); it != sClients.end(); it++)
                {
                    if (it->second.getAddr() == addrInt && it->second.getPort() == portInt)
                    {
                        string sendMsg = to_string(sClient) + "(" + to_string(it->second.getAddr()) + " " + to_string(it->second.getPort()) + ")" + ": " + content;
                        sendString(it->first, sendMsg.c_str(), MESSAGE);
                        flag = true;
                        break;
                    }
                }
                if (!flag)
                {
                    sendString(sClient, "[SERVER] No such client.", _ERROR);
                    cout << "[INFO] Failed!\n\n";
                    break;
                }
                else
                {
                    sendString(sClient, "[SERVER] Message sent successfully.");
                    cout << "[INFO] Success!\n\n";
                    break;
                }
            }
            case CLOSE:
            {
                /* 关闭连接 */
                sendString(sClient, "[SERVER] Connection closed.", CLOSE);
                cout << "[INFO] Connection closed by client.\n\n";
                sClients.erase(sClient);
                ReleaseMutex(mutex);
                cout << "-----------------------------\n";
                cout << "Connection from " << sClient << " closed.\n";
                cout << "-----------------------------\n";
                closesocket(sClient);
                return 0;
            }
            case EXIT:
            {
                sendString(sClient, "[SERVER] Connection closed.", EXIT);
                cout << "[INFO] Connection closed by client.\n\n";
                sClients.erase(sClient);
                ReleaseMutex(mutex);
                cout << "-----------------------------\n";
                cout << "Client " << sClient << " exited.\n";
                cout << "-----------------------------\n";
                closesocket(sClient);
                return 0;
            }
            }
            ReleaseMutex(mutex);
        }
    }
    cout << "-----------------------------\n";
    cout << "Connection from " << sClient << " closed.\n";
    cout << "-----------------------------\n";
    closesocket(sClient);
    return 0;
}

DWORD WINAPI ThreadProc2(LPVOID lp)
{
    while (true)
    {
        WaitForSingleObject(mutex, INFINITE);
        string input;
        cin >> input;
        if (input == "exit")
        {
            message pack;
            pack.type = EXIT;
            memcpy(pack.data, "Server is shutting down.", 1024);
            for (auto it = sClients.begin(); it != sClients.end(); it++)
            {
                send(it->first, (char *)&pack, sizeof(pack), 0);
                closesocket(it->first);
            }
            closesocket(srvSock);
            cout << "[INFO] Server is shutting down.\n";
            return 0;
        }
        ReleaseMutex(mutex);
    }
    return 0;
}

int main() {
    WSADATA wsadata;
    WORD rqstword = MAKEWORD(2, 2);
    if (WSAStartup(rqstword, &wsadata) != 0) {
        cout << "[ERROR] WSAStartup failed" << endl;
        return 1;
    }
    // 申请socket句柄
    srvSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srvSock == INVALID_SOCKET) {
        cout << "[ERROR] Invalid Socket!" << endl;
        return 1;
    }
    BOOL reuseAddr = TRUE;
    setsockopt(srvSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseAddr, sizeof(reuseAddr));
    // bind
    sockaddr_in srvAddr;
    memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_family = PF_INET;
    srvAddr.sin_port = htons(SPORT);
    srvAddr.sin_addr.s_addr = inet_addr(SIP);
    if (bind(srvSock, (SOCKADDR *)&srvAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
        cout << "[ERROR] Bind failed" << endl;
        return 1;
    }
    // listen
    if (listen(srvSock, 20)) {
        cout << "[ERROR] Listen failed" << endl;
        return 1;
    }
    cout << "[INFO] Server initialized, waiting for connection...\n";

    HANDLE eThread = CreateThread(NULL, 0, ThreadProc2, NULL, 0, NULL);
    while (true)
    {
        sockaddr_in clntAddr;
        int Len = sizeof(clntAddr);
        SOCKET clntSock = accept(srvSock, (sockaddr *)&clntAddr, &Len);
        if (clntSock == INVALID_SOCKET)
            continue;
        sClients[clntSock] = sClientInfo(PF_INET, ntohs(clntAddr.sin_port), ntohl(clntAddr.sin_addr.S_un.S_addr));
        HANDLE tThread = CreateThread(NULL, 0, ThreadProc, &clntSock, 0, NULL);
    }
    return 0;
}