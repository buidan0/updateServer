#ifndef _UPGRADESERVER_H_
#define _UPGRADESERVER_H_

#include "ev.h"
#include <iostream>
#include <netinet/in.h>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
//#include <Winsock2.h>
#include <cstdio>
#include "tinyxml.h"
#include "protocol.h"
#include "spdlog/sinks/rotating_file_sink.h"


using namespace std;

class ClientSocket
{
public:
	ClientSocket()
	{
		//HeartBeatFlag = 0;
		client_sd = 0;
		//heartBeatTimer = NULL;
		enable = false;
		lastGetHBTime = 0;
		username = "";
	}
	//int HeartBeatFlag;
	int client_sd;
	struct sockaddr_in client_addr;
	struct ev_io client_watcher;

	bool GetEnable() const
	{
		return enable;
	}
	void setEnable()
	{
		enable = true;
	}
	void disable()
	{
		enable = false;
		username = "";
	}
	~ClientSocket()
	{
		//if (heartBeatTimer)
		//	free ( heartBeatTimer );

	}
	time_t lastGetHBTime;
	string username;
private:

	bool enable;

};

class UpgradeServer
{
#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 1
#define MAX_CLIENTNUMS 10
#define HB_TIMEMOUT 15

private:
	int sd;
	int _SERVER_PORT;
    std::shared_ptr<spdlog::logger> logger = spdlog::rotating_logger_mt("", "UpdateServer.log", 1048576 * 100, 3);
	struct ev_loop *mainloop;
	struct ev_io *socket_watcher;
	ev_timer* heartBeatTimer;	//������ⶨʱ��
	char recvBuffer[BUFFER_SIZE];	//���ջ���
	char sendBuffer[BUFFER_SIZE];	//���ͻ���
	ClientSocket ClientSocketList[MAX_CLIENTNUMS];
	//string FTPServerCfgFile;
	//string FTPServerUserListFile;
	int clientCount;

	//�����û����б�
	vector<string> UserList;

	/*socket������*/
	void socket_accept_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);

	void socket_read_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);

private:

	void on_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents);
	int CloseClientSocket(int client_fd);
	int GetClientSocketIndex(int client_fd);
	int GetUserList();
	bool IsValidUser(const char* username);
	int sendMessage(int fd, int result, int cmdtype, const char* token);

	/*FTP������*/
	int SetFtpServerPriotyToUser(const char* username);
	int ReCoverFtpServerPrioty();
	int RestartFtpServer();
	/*��Ϣ���캯��*/
	MessageHeader BuildMessageHeader(int cmd, int len);
	const char*BuildLoginRespMessage(int result, const char* token);
	const char*BuildLogoutRespMessage(int result, const char* token);
	const char*BuildUploadRespMessage(int result, const char* token);

	/*��Ϣ������*/
	int checkHeader(MessageHeader& header) ;
	int NtohHeader(MessageHeader& header);
	int ProcessHearBeat(int clentSocketIndex, const char* msg);
	int ProcessLogin(int clientSocketIndex, const char*msg);
	int ProcessLogout(int clientSocketIndex, const char* msg);
	int ProcessUploadDone(int clientSocketIndex, const char*msg);
	int ProcessUploadRequest(int clientSocketIndex, const char*msg);
	//�ص�����
	static UpgradeServer* CurMy;    //�洢�ص��������õĶ���
	static void read_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);    //�ص�����
	static void accept_callback(struct ev_loop *loop, struct ev_io *watcher, int revents);    //�ص�����
	static void on_timerout_callback(struct ev_loop *loop, ev_timer *w, int revents);
public:
	~UpgradeServer();
	explicit  UpgradeServer(int port);
	void start();

};

#endif
