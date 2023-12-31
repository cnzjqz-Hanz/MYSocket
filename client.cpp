#include <iostream>
#include <iomanip>
#include <string>
#include <WinSock2.h>
#include <Windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

enum RQ_TYPE
{
    STRING = 0x1,
    _ERROR,
    TIME,
    NAME,
    CLIENT_LIST,
    MESSAGE,
    CLOSE,
    EXIT
};

SOCKET sClient;
bool isConnected = false;


struct message
{
    RQ_TYPE type;
    char data[1024] = "";
};

void showMenu(bool isConnected = false)
{
    cout << "-----------------------------\n";
    cout << "[MENU]\n";
    cout << "Options:\n";
    if (isConnected)
    {
        cout << "[0] Get Server Time.\n";
        cout << "[1] Get Server Name.\n";
        cout << "[2] Get Client List.\n";
        cout << "[3] Send Message to other client.\n";
        cout << "[4] Close the connection.\n";
        cout << "[5] Exit the client.\n";
    }
    else
    {
        cout << "[0] Connect to the server.\n";
        cout << "[1] Exit the client.\n";
    }
    cout << "-----------------------------\n";
    cout << "Please enter your option: ";
}

DWORD WINAPI RecvProc(LPVOID lp)
{
    while (true)
    {
        message recvPack;
        if (recv(sClient, (char *)&recvPack, sizeof(recvPack), 0) != SOCKET_ERROR)
        {
            switch (recvPack.type)
            {
            case STRING:
                cout << "\n[INFO] " << recvPack.data << endl;
                break;
            case _ERROR:
                cout << "\n[ERROR] " << recvPack.data << endl;
                break;
            case TIME:
                cout << "\n[INFO] Server time: " << recvPack.data << endl;
                break;
            case NAME:
                cout << "\n[INFO] Server name: " << recvPack.data << "\n"
                     << endl;
                break;
            case CLIENT_LIST:
                cout << "\n[INFO] Client list: \n" << recvPack.data << "\n"
                     << endl;
                break;
            case MESSAGE:
                cout << "\n[INFO] Message from " << recvPack.data << endl;
                cout << "\nPlease enter your option: ";
                break;
            case CLOSE:
                cout << "\n[INFO] Connection closed.\n"
                     << endl;
                return 0;
            case EXIT:
                cout << "\n[INFO] Exit.\n"
                     << endl;
                return 0;
            }
        }
        else
        {
            cout << "\n[ERROR] Connection lost.\n"
                 << endl;
            return 0;
        }
    }
}

void run(void) {
    bool flag = true;
    while (flag) {
        showMenu(isConnected);
        int opt;
        cin >> opt;
        if ((opt < 0 || opt > 5)) {
            cout << "[ERROR] Invalid option.\n";
            continue;
        }
        switch (opt) {
        case 0:
            if (isConnected) {
                message sendPack;
                sendPack.type = TIME;
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
            }
            else {
                sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sClient == INVALID_SOCKET) {
                    cout << "[ERROR] socket failed" << WSAGetLastError() << endl;
                    return;
                }
                string input;
                cout << "> Please enter the server IP address: ";
                cin >> input;
                auto ip = inet_addr(input.c_str());
                if (ip == INADDR_NONE) {
                    cout << "[ERROR] Invalid IP. Try again!\n";
                    break;
                }
                string port;
                cout << "> Please enter the server port number: ";
                cin >> port;
                int portNum = stoi(port);
                if (portNum < 0 || portNum > 65535) {
                    cout << "[ERROR] Invalid port number. Try again!\n";
                    break;
                }
                sClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                struct sockaddr_in sockAddr;
                memset(&sockAddr, 0, sizeof(sockAddr));
                sockAddr.sin_family = PF_INET;
                sockAddr.sin_addr.s_addr = ip;
                sockAddr.sin_port = htons(portNum);
                if (connect(sClient, (SOCKADDR *)&sockAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
                    cout << "[ERROR] Fail to connect to the server. Try again later!\n";
                    closesocket(sClient);
                }
                else {
                    cout << "[INFO] Connected to the server.\n";
                    isConnected = true;
                    CreateThread(NULL, 0, RecvProc, NULL, 0, NULL);
                    Sleep(200);
                }
            }
            break;
        case 1:
            if (isConnected) {
                message sendPack;
                sendPack.type = NAME;
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
            }
            else {
                flag = false;
                cout << "[INFO] Shut down...\n";
            }
            break;
        case 2:
            if (isConnected) {
                message sendPack;
                sendPack.type = CLIENT_LIST;
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
            }
            else {
                cout << "[ERROR] Not connected yet.\n";
                flag = false;
            }
            break;
        case 3:
            if (isConnected) {
                message sendPack;
                sendPack.type = MESSAGE;
                string caddr, cport, cmsg;
                cout << "> Please enter the client address: ";
                cin >> caddr;
                cout << "> Please enter the client port: ";
                cin >> cport;
                cout << "> Please enter the message: ";
                cin >> cmsg;
                string msg_pack = caddr + ":" + cport + " " + cmsg;
                strcpy(sendPack.data, msg_pack.c_str());
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
            }
            else {
                cout << "[ERROR] Not connected yet.\n";
                flag = false;
            }
            break;
        case 4:
            if (isConnected) {
                message sendPack;
                sendPack.type = CLOSE;
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
                isConnected = false;
            }
            else {
                cout << "[ERROR] Not connected yet.\n";
                flag = false;
            }
            break;
        case 5:
            if (isConnected) {
                message sendPack;
                sendPack.type = EXIT;
                send(sClient, (char *)&sendPack, sizeof(sendPack), 0);
                Sleep(200);
                isConnected = false;
                flag = false;
                cout << "[INFO] Shut down...\n";
            }
            else {
                cout << "[ERROR] Not connected yet.\n";
                flag = false;
            }
            break;
        default:
            cout << "[ERROR] Invalid option.\n";
            break;
        }
    }

}


int main() {
    WSADATA wsadata;
    WORD rqstword = MAKEWORD(2, 2);
    if (WSAStartup(rqstword, &wsadata) != 0) {
        cout << "[ERROR] WSAStartup failed" << WSAGetLastError() << endl;
        return 1;
    }
    run();
    WSACleanup();
    return 0;
}