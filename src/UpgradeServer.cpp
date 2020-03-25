#include "UpgradeServer.h"

using namespace std;

UpgradeServer::UpgradeServer(int port = 60000) {
    try {
        _SERVER_PORT = port;
        mainloop = ev_default_loop(0);
        socket_watcher = (struct ev_io *) malloc(sizeof(struct ev_io));
        heartBeatTimer = (ev_timer *) malloc(sizeof(ev_timer));
        sd = 0;
        clientCount = 0;
        memset(recvBuffer, 0, BUFFER_SIZE);
        memset(sendBuffer, 0, BUFFER_SIZE);
        CurMy = this;
        GetUserList();
        logger->flush_on(spdlog::level::debug); //设置日志立即输出级别，高于或者等于该级别就立即输出

    } catch (std::exception &e) {
        logger->error(e.what());
    }

}

void UpgradeServer::start() {

    try {
        logger->info("---------------server start ------------------");
        //��ʼ������������
        ReCoverFtpServerPrioty();
        RestartFtpServer();
        int sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd < 0) {
            logger->error("socket fail");
            exit(-1);
        }
        logger->info("socket ok");
        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons (_SERVER_PORT);
        addr.sin_addr.s_addr = INADDR_ANY;

        // set sd reuseful
        int bReuseaddr = 1;
        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *) &bReuseaddr, sizeof(bReuseaddr)) != 0) {
            logger->info("setsockopt error in reuseaddr[{0:d}],exit", sd);
            exit(-1);
        }
        logger->info("listen ok");

        if (bind(sd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
            logger->error("bind error,exit");
            exit(-1);
        }
        logger->info("bind ok");
        if (listen(sd, 1) < 0) {
            logger->error("listen error,exit");
            exit(-1);
        }

        /*����socket�˿�*/
        ev_io_init(socket_watcher, accept_callback, sd, EV_READ);
        logger->info("ev_io_init ok");
        ev_io_start(mainloop, socket_watcher);
        logger->info("ev_io_start ok");
        /*����������鶨ʱ��*/
        ev_timer_init(heartBeatTimer, on_timerout_callback, 0, 1);
        ev_timer_start(mainloop, heartBeatTimer);
        ev_run(mainloop, 0);
    }
    catch (...) {
        logger->info("some error happen,exit");
        exit(-1);
    }

}

void UpgradeServer::socket_accept_callback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    socklen_t client_len = sizeof(struct sockaddr_in);
    if (EV_ERROR & revents) {
        logger->error("error event in accept");
        return;
    }
    // socket accept: get file description

    ClientSocketList[clientCount].client_sd = accept(watcher->fd,
                                                     (struct sockaddr *) &ClientSocketList[clientCount].client_addr,
                                                     &client_len);
    if (ClientSocketList[clientCount].client_sd < 0) {
        logger->error("accept error");
        return;
    }
    int indexnow = clientCount;
    //��������һ
    clientCount++;
    // too much connections
    if (clientCount > MAX_CONNECTIONS) {
        logger->error("fd too large");
        close(ClientSocketList[indexnow].client_sd);
        clientCount--;
        return;
    }

    ClientSocketList[indexnow].setEnable();

    //����ʱ��
    ClientSocketList[indexnow].lastGetHBTime = time(nullptr);

    logger->info("client connected");
    // listen new client
    ev_io_init(&(ClientSocketList[indexnow].client_watcher), read_callback, ClientSocketList[indexnow].client_sd,
               EV_READ);
    ev_io_start(loop, &ClientSocketList[indexnow].client_watcher);

}

void UpgradeServer::socket_read_callback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    int index = 0;
    index = GetClientSocketIndex(watcher->fd);
    if (index < 0) {
        logger->info("error  in GetClientSocketIndex ");
        return;
    }
    memset(recvBuffer, 0, sizeof(recvBuffer));
    ssize_t read;
    if (EV_ERROR & revents) {
        logger->info("error event in read");
        return;
    }
    // socket recv
    read = recv(watcher->fd, recvBuffer, BUFFER_SIZE, 0); // read stream to buffer
    if (read < 0) {
        logger->info("read error");
        return;
    }

    if (read == 0) {
        logger->info("client disconnected.");

        CloseClientSocket(watcher->fd);
        return;
    } else {
        logger->info("receive message");

        MessageHeader header;
        memcpy((void *) &header, recvBuffer, sizeof(MessageHeader));
        NtohHeader(header);
        if (OK == checkHeader(header)) {
            char msgbody[1024];
            logger->debug(" header.xmlLen:{0:d}",header.xmlLen);
            //*logger << log4cpp::Priority::DEBUG << " header.xmlLen:" << header.xmlLen;
            memcpy(msgbody, recvBuffer + sizeof(MessageHeader), header.xmlLen);
            switch (header.command) {
                case LoginRequest :
                    logger->info("receive LoginRequest");
                    ProcessLogin(index, msgbody);
                    break;
                case LogoutRequest :
                    //*logger << log4cpp::Priority::NOTICE << "receive LogoutRequest";
                    logger->info("receive LogoutRequest");
                    ProcessLogout(index, msgbody);
                    break;
                case HeartBeatRequst :
                    //*logger << log4cpp::Priority::NOTICE << "receive heartBeat";
                    logger->info("receive heartBeat");
                    ProcessHearBeat(index, msgbody);
                    break;
                case UploadRequest :
                    //*logger << log4cpp::Priority::NOTICE << "receive UploadRequest";
                    logger->info("receive UploadRequest");
                    ProcessUploadRequest(index, msgbody);
                    break;
                case UploadDone :
                    //*logger << log4cpp::Priority::NOTICE << "receive UploadDone";
                    logger->info("receive UploadDone");
                    ProcessUploadDone(index, msgbody);
                    break;
                default :
                    //*logger << log4cpp::Priority::NOTICE << "other command:" << header.command;
                    logger->info("receive UploadRequest:{0:ud}",header.command);
                    break;
            }

        } else {
            //*logger << log4cpp::Priority::ERROR << "invalid message";
            logger->error("invalid message");
        }

    }
}

void UpgradeServer::on_timeout_cb(struct ev_loop *loop, ev_timer *w, int revents) {
    int i = 0;
    time_t now_time = time(nullptr);
    for (i = 0; i < clientCount; i++) {
        if (ClientSocketList[i].GetEnable()) {
            if ((now_time - ClientSocketList[i].lastGetHBTime) > HB_TIMEMOUT) {
                //*logger << log4cpp::Priority::NOTICE << "close cocket " << ClientSocketList[i].client_sd;
                logger->info("close cocket:{0:d}",ClientSocketList[i].client_sd);
                CloseClientSocket(ClientSocketList[i].client_sd);
            }
        }
    }

}

int UpgradeServer::CloseClientSocket(int client_fd) {
    /*����clientSocket��������*/

    int index = GetClientSocketIndex(client_fd);
    if (index < 0) {
        close(client_fd);
        return -1;
    }
    //*logger << log4cpp::Priority::NOTICE << "index=" << index;
    //logger->info("index=:{0:d}",index);
    //CloseClientSocket(ClientSocketList[index].client_sd);
    clientCount--;

    //ȡ��ʹ��
    ClientSocketList[index].disable();

    //ֹͣ�˿ڼ���
    ev_io_stop(mainloop, &ClientSocketList[index].client_watcher);
    //*logger << log4cpp::Priority::NOTICE << "stop client_watcher";
    //*logger << log4cpp::Priority::NOTICE << "stop client_watcher";
    logger->info("stop client_watcher");

    //�ָ�������Ȩ�޲�����,��ֻ��һ���ͻ��˵�ʱ���ǿ��еģ�����ж���ᵼ���ظ�������
    ReCoverFtpServerPrioty();
    RestartFtpServer();

    //�ر�socket
    close(client_fd);
    return 0;
}

int UpgradeServer::GetClientSocketIndex(int client_fd) {
    int i = 0;
    for (i = 0; i < MAX_CLIENTNUMS; i++) {
        if (ClientSocketList[i].client_sd == client_fd)
            return i;
    }
    return -1;
}

UpgradeServer::~UpgradeServer() {
    ev_io_stop(mainloop, socket_watcher);
    ev_timer_stop(mainloop, heartBeatTimer);
    free(socket_watcher);
    socket_watcher = nullptr;
    ev_loop_destroy(mainloop);
    free(mainloop);
    mainloop = nullptr;

}

int UpgradeServer::SetFtpServerPriotyToUser(const char *username) {
    //����vsconfg�ļ�
    FILE *fp = popen("sed  -i 's\\userlist_deny=YES\\userlist_deny=NO\\g' /etc/vsftpd/vsftpd.conf", "r");
    if (nullptr == fp) {
        //logger->error("set userlist_deny=NO fail");
        logger->error("set userlist_deny=NO fail");
        pclose(fp);
        return -1;
    } else {
        //*logger << log4cpp::Priority::NOTICE << "set userlist_deny=NO OK";
        logger->info("set userlist_deny=NO fail");
    }
    //ֻ��user����Ϊ�����û�
    char cmd[100];
    memset(cmd, 0, 100);
    snprintf(cmd, 100, "sed -i '$a%s' /etc/vsftpd/user_list", username);
    fp = popen(cmd, "r");
    if (nullptr == fp) {
        logger->error("add user  fail");
        pclose(fp);
        return -1;
    } else {
        //*logger << log4cpp::Priority::NOTICE << "add user right " << username;
        logger->info("add user right {0}",username);
    }
    pclose(fp);
    return 0;
}

int UpgradeServer::ReCoverFtpServerPrioty() {
    //����vsconfg�ļ�
    FILE *fp = popen("sed  -i 's\\userlist_deny=NO\\userlist_deny=YES\\g' /etc/vsftpd/vsftpd.conf", "r");
    if (nullptr == fp) {
        logger->error("set userlist_deny=YES fail");
        pclose(fp);
        return -1;
    } else {
        //*logger << log4cpp::Priority::NOTICE << "set userlist_deny=YES OK";
        logger->info("set userlist_deny=YES OK");
    }

    //ɾ��������ӵ��û�
    fp = popen("sed -i '6,$d' /etc/vsftpd/user_list", "r");
    if (nullptr == fp) {
        logger->error("del user  fail");
        pclose(fp);
        return -1;
    } else {
        //*logger << log4cpp::Priority::NOTICE << "del user right ";
        logger->info("del user right ");
    }
    pclose(fp);
    return 0;
}

int UpgradeServer::RestartFtpServer() {
    char line[100];
    FILE *fp = popen("service vsftpd stop", "r");
    if (nullptr == fp) {
        logger->error("restart fail");
        pclose(fp);
        return -1;
    } else {
        if (fgets(line, 100, fp) != nullptr) {
            //cout<<line;
            if (nullptr != strstr(line, "OK")) {
                //*logger << log4cpp::Priority::NOTICE << "ftp  close ok";
                logger->info("ftp  close ok");
            } else
                //*logger << log4cpp::Priority::NOTICE << "close fail";
                logger->info("close fail");
        }
    }
    fp = popen("service vsftpd start", "r");
    if (nullptr == fp) {
        logger->error("restart fail");
        pclose(fp);
        return -1;
    } else {
        if (fgets(line, 100, fp) != nullptr) {
            if (nullptr != strstr(line, "OK")) {
                //*logger << log4cpp::Priority::NOTICE << "ftp start ok";
                logger->info("ftp start ok");
            } else {
                //*logger << log4cpp::Priority::NOTICE << "start fail";
                logger->info("start fail");
                pclose(fp);
                return -1;
            }

        }
    }
    pclose(fp);
    return 0;
}

MessageHeader UpgradeServer::BuildMessageHeader(int cmd, int len) {
    MessageHeader header;
    header.command = htons (cmd);
    header.encryptType = 0;
    header.magicValue = htonl (0x79183351);
    header.version = htons (0x0001);
    header.xmlLen = htonl (len);
    return header;
}

const char *UpgradeServer::BuildLoginRespMessage(int result, const char *token) {
    string xmlstr;
    TiXmlPrinter printer;
    // ����һ��XML���ĵ�����
    TiXmlDocument *xmlDocument = new TiXmlDocument();
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    xmlDocument->LinkEndChild(decl);
    // ��Ԫ�ء�
    TiXmlElement *rootElement = new TiXmlElement(ProtocolDef::Reply);
    // ���ӵ��ĵ�����, ��Ϊ��Ԫ��
    xmlDocument->LinkEndChild(rootElement);
    // ����tokenԪ�غ�resultԪ��
    TiXmlElement *tokenElement = new TiXmlElement(ProtocolDef::Token);
    TiXmlElement *resultElement = new TiXmlElement(ProtocolDef::Result);
    // ����tokenԪ�غ�resultԪ�ص�ֵ��

    TiXmlText *numberValue = new TiXmlText(token);
    tokenElement->LinkEndChild(numberValue);
    char temp[25];
    snprintf(temp, sizeof(temp), "%d", result);
    TiXmlText *resultValue = new TiXmlText(temp);
    resultElement->LinkEndChild(resultValue);
    // ����
    rootElement->LinkEndChild(tokenElement);
    rootElement->LinkEndChild(resultElement);
    xmlDocument->Accept(&printer);
    xmlstr = printer.CStr();
    xmlDocument->Clear();
    delete xmlDocument;
    return xmlstr.c_str();
}

const char *UpgradeServer::BuildLogoutRespMessage(int result, const char *token) {
    string xmlstr;
    TiXmlPrinter printer;
    // ����һ��XML���ĵ�����
    TiXmlDocument *xmlDocument = new TiXmlDocument();
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    xmlDocument->LinkEndChild(decl);
    // ��Ԫ�ء�
    TiXmlElement *rootElement = new TiXmlElement("reply");
    // ���ӵ��ĵ�����, ��Ϊ��Ԫ��
    xmlDocument->LinkEndChild(rootElement);
    // ����tokenԪ�غ�resultԪ��
    TiXmlElement *tokenElement = new TiXmlElement("token");
    TiXmlElement *resultElement = new TiXmlElement("result");
    // ����tokenԪ�غ�resultԪ�ص�ֵ��
    TiXmlText *numberValue = new TiXmlText(token);
    tokenElement->LinkEndChild(numberValue);
    char temp[25];
    snprintf(temp, sizeof(temp), "%d", result);
    TiXmlText *resultValue = new TiXmlText(temp);
    resultElement->LinkEndChild(resultValue);
    // ����
    rootElement->LinkEndChild(tokenElement);
    rootElement->LinkEndChild(resultElement);
    xmlDocument->Accept(&printer);
    xmlstr = printer.CStr();
    xmlDocument->Clear();
    delete xmlDocument;
    return xmlstr.c_str();
}

const char *UpgradeServer::BuildUploadRespMessage(int result, const char *token) {
    string xmlstr;
    TiXmlPrinter printer;
    // ����һ��XML���ĵ�����
    TiXmlDocument *xmlDocument = new TiXmlDocument();
    TiXmlDeclaration *decl = new TiXmlDeclaration("1.0", "UTF-8", "");
    xmlDocument->LinkEndChild(decl);
    // ��Ԫ�ء�
    TiXmlElement *rootElement = new TiXmlElement("reply");
    // ���ӵ��ĵ�����, ��Ϊ��Ԫ��
    xmlDocument->LinkEndChild(rootElement);
    // ����tokenԪ�غ�resultԪ��
    TiXmlElement *tokenElement = new TiXmlElement("token");
    TiXmlElement *resultElement = new TiXmlElement("result");
    // ����tokenԪ�غ�resultԪ�ص�ֵ��
    TiXmlText *numberValue = new TiXmlText(token);
    tokenElement->LinkEndChild(numberValue);
    char temp[25];
    snprintf(temp, sizeof(temp), "%d", result);
    TiXmlText *resultValue = new TiXmlText(temp);
    resultElement->LinkEndChild(resultValue);
    // ����
    rootElement->LinkEndChild(tokenElement);
    rootElement->LinkEndChild(resultElement);
    xmlDocument->Accept(&printer);
    xmlstr = printer.CStr();
    xmlDocument->Clear();
    delete xmlDocument;
    return xmlstr.c_str();
}

int UpgradeServer::ProcessHearBeat(int clientSocketIndex, const char *msg) {
    //������Ϣ����
    //����һ��XML���ĵ�����
    TiXmlDocument *myDocument = new TiXmlDocument();
    //-------------��ȡ�ַ���-----------
    myDocument->Parse(msg, nullptr, TIXML_ENCODING_UTF8);
    if (myDocument) {
        //��ø�Ԫ�ء�
        TiXmlElement *RootElement = myDocument->RootElement();
        if (RootElement) {
            //�����Ԫ�����ơ�
            string str = ProtocolDef::Request;
            if (0 == str.compare(RootElement->Value())) {
                //��¼����ʱ��
                ClientSocketList[clientSocketIndex].lastGetHBTime = time(
                        nullptr);
            }
        }

    }
    myDocument->Clear();
    delete myDocument;
    return 0;
}

int UpgradeServer::ProcessLogin(int clientSocketIndex, const char *msg) {
    //const char *msgbody = nullptr;
    //MessageHeader header;
    //������Ϣ����
    //����һ��XML���ĵ�����
    TiXmlDocument *myDocument = new TiXmlDocument();
    //-------------��ȡ�ַ���-----------
    myDocument->Parse(msg, nullptr, TIXML_ENCODING_UTF8);
    if (myDocument) {
        //��ø�Ԫ�ء�
        TiXmlElement *RootElement = myDocument->RootElement();
        if (RootElement) {
            //�����Ԫ�����ơ�
            string str = ProtocolDef::Request;
            if (0 == str.compare(RootElement->Value())) {
                //��õ�һ���ڵ㡣
                TiXmlElement *FirstPerson = RootElement->FirstChildElement();
                if (FirstPerson) {
                    //��õ�һ���ڵ���ӽڵ��ֵ
                    const char *token = FirstPerson->GetText();
                    //*logger << log4cpp::Priority::DEBUG << "token:" << token;
                    logger->debug("token:{0}",token);
                    //��ȡ��һ��ͬ��Ԫ��
                    TiXmlElement *account = FirstPerson->NextSiblingElement();
                    if (account) {
                        //*logger << log4cpp::Priority::DEBUG << account->GetText();
                        logger->debug("{0}",account->GetText());
                        //����û����������Ч�򷵻ص�¼ʧ��
                        if (!IsValidUser(account->GetText())) {
                            sendMessage(ClientSocketList[clientSocketIndex].client_sd, FAIL, LoginReply, token);
                        } else {
                            //�����û���
                            ClientSocketList[clientSocketIndex].username = account->GetText();

                            //�ظ���¼�ɹ�
                            sendMessage(ClientSocketList[clientSocketIndex].client_sd, OK, LoginReply, token);
                        }

                    }

                }

            }

        }

    }

    myDocument->Clear();
    delete myDocument;
    return 0;
}

int UpgradeServer::ProcessLogout(int clientSocketIndex, const char *msg) {
    //const char *msgbody = nullptr;
    //MessageHeader header;
    //������Ϣ����
    //����һ��XML���ĵ�����
    TiXmlDocument *myDocument = new TiXmlDocument();
    //-------------��ȡ�ַ���-----------
    myDocument->Parse(msg, nullptr, TIXML_ENCODING_UTF8);
    //��ø�Ԫ�ء�
    TiXmlElement *RootElement = myDocument->RootElement();
    //�����Ԫ�����ơ�
    string str = ProtocolDef::Request;
    if (0 == str.compare(RootElement->Value())) {
        //��õ�һ���ڵ㡣
        TiXmlElement *FirstPerson = RootElement->FirstChildElement();
        if (FirstPerson) {
            //��õ�һ���ڵ���ӽڵ��ֵ
            const char *token = FirstPerson->GetText();
            //*logger << log4cpp::Priority::DEBUG << token;
            logger->debug("{0}",token);
            //�ظ��ǳ��ɹ�
            //sendMessage ( ClientSocketList[clientSocketIndex].client_sd , OK , LogoutReply , token );
            //�Ͽ��ͻ���
            CloseClientSocket(ClientSocketList[clientSocketIndex].client_sd);
        }

    }

    myDocument->Clear();
    delete myDocument;
    return 0;
}

int UpgradeServer::ProcessUploadDone(int clientSocketIndex, const char *msg) {
    //������Ϣ����
    //����һ��XML���ĵ�����
    TiXmlDocument *myDocument = new TiXmlDocument();
    //-------------��ȡ�ַ���-----------
    myDocument->Parse(msg, nullptr, TIXML_ENCODING_UTF8);
    if (myDocument) {
        //��ø�Ԫ�ء�
        TiXmlElement *RootElement = myDocument->RootElement();
        if (RootElement) {
            //�����Ԫ�����ơ�
            string str = ProtocolDef::Request;
            if (0 == str.compare(RootElement->Value())) {
                //��õ�һ���ڵ㡣
                TiXmlElement *FirstPerson = RootElement->FirstChildElement();
                if (FirstPerson) {
                    //��õ�һ���ڵ���ӽڵ��ֵ
                    const char *token = FirstPerson->GetText();
                    //*logger << log4cpp::Priority::DEBUG << token;
                    logger->debug("{0}",token);
                    //�л�FTP��������Ĭ�ϵ�Ȩ�ޣ�������FTP��������
                    ReCoverFtpServerPrioty();
                    RestartFtpServer();

                }

            }

        }

    }

    myDocument->Clear();
    delete myDocument;
    return 0;
}

int UpgradeServer::ProcessUploadRequest(int clientSocketIndex, const char *msg) {
//    const char *msgbody = nullptr;
//    MessageHeader header;
    //������Ϣ����
    //����һ��XML���ĵ�����
    TiXmlDocument *myDocument = new TiXmlDocument();
    //-------------��ȡ�ַ���-----------
    myDocument->Parse(msg, nullptr, TIXML_ENCODING_UTF8);
    if (myDocument) {
        //��ø�Ԫ�ء�
        TiXmlElement *RootElement = myDocument->RootElement();
        //�����Ԫ�����ơ�
        string str = ProtocolDef::Request;
        if (0 == str.compare(RootElement->Value())) {
            //��õ�һ���ڵ㡣
            TiXmlElement *FirstPerson = RootElement->FirstChildElement();
            if (FirstPerson) {
                //��õ�һ���ڵ���ӽڵ��ֵ
                const char *token = FirstPerson->GetText();
                //*logger << log4cpp::Priority::DEBUG << token;
                logger->debug("{0}",token);
                //�л�FTP��������Ȩ�ޣ�ֻ������û������ʣ�������FTP��������
                int res = SetFtpServerPriotyToUser(ClientSocketList[clientSocketIndex].username.c_str());
                cout << "SetFtpServerPriotyToUser:" << res << endl;
                res = RestartFtpServer();

                //����л�ʧ�ܣ���Ӧʧ����Ϣ
                if (res != 0) {
                    //�ظ�ʧ��
                    sendMessage(ClientSocketList[clientSocketIndex].client_sd, FAIL, UploadReply, token);
                } else {
                    //����л��ɹ�����Ӧ��׼����������Ϣ��
                    sendMessage(ClientSocketList[clientSocketIndex].client_sd, OK, UploadReply, token);
                }
            }
        }

    }

    myDocument->Clear();
    delete myDocument;
    return 0;

}

int UpgradeServer::checkHeader(MessageHeader &header) {
    if (header.magicValue == 0x79183351)
        return OK;
    else
        return FAIL;
}

int UpgradeServer::GetUserList() {
    char line[256];
    FILE *fp = popen("cat /etc/passwd", "r");
    if (nullptr == fp) {
        logger->error("restart fail\n");
        pclose(fp);
        return -1;
    } else {
        while (fgets(line, 256, fp) != nullptr) {
            string temp = line;
            int found = temp.find(':');
            if (found != std::string::npos) {
                string username = temp.substr(0, found);
                UserList.push_back(username);
            }
        }
    }

    pclose(fp);
    return 0;
}

bool UpgradeServer::IsValidUser(const char *username) {
    vector<string>::iterator it = UserList.begin();
    it = find(UserList.begin(), UserList.end(), username);
    return (it != UserList.end());
}

int UpgradeServer::sendMessage(int fd, int result, int cmdtype, const char *token) {
    const char *msgbody = nullptr;
    switch (cmdtype) {
        case LoginReply :
            msgbody = BuildLoginRespMessage(result, token);
            break;
        case LogoutReply :
            msgbody = BuildLogoutRespMessage(result, token);
            break;
        case UploadReply :
            msgbody = BuildUploadRespMessage(result, token);
            break;
        default :
            //*logger << log4cpp::Priority::NOTICE << "invalid cmdtype" << cmdtype;
            logger->error("invalid cmdtype:{0:d}", cmdtype);
            break;

    }

    if (msgbody == nullptr) {
        //*logger << log4cpp::Priority::NOTICE << "msgbody is nullptr";
        logger->info("msgbody is nullptr");
        return -1;
    }

    MessageHeader header = BuildMessageHeader(cmdtype, strlen(msgbody) + 1);
    memset(sendBuffer, 0, sizeof(sendBuffer));
    memcpy(sendBuffer, &header, sizeof(MessageHeader));
    memcpy(sendBuffer + sizeof(MessageHeader), msgbody, strlen(msgbody) + 1);
    int res = send(fd, sendBuffer, BUFFER_SIZE, 0); //
    if (res <= 0) {
        //*logger << log4cpp::Priority::NOTICE << "send error";
        logger->info("send error");
        return -1;
    } else {
        //*logger << log4cpp::Priority::NOTICE << "send replyMessage ok";
        logger->info("send replyMessage ok");
        return 0;
    }
}

int UpgradeServer::NtohHeader(MessageHeader &header) {
    header.magicValue = ntohl (header.magicValue);
    header.encryptType = ntohs (header.encryptType);
    header.command = ntohs (header.command);
    header.xmlLen = ntohl (header.xmlLen);
    header.version = ntohs (header.version);
    return 0;
}

UpgradeServer *UpgradeServer::CurMy = nullptr;

void UpgradeServer::read_callback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    UpgradeServer::CurMy->socket_read_callback(loop, watcher, revents);
}

void UpgradeServer::accept_callback(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    UpgradeServer::CurMy->socket_accept_callback(loop, watcher, revents);
}

void UpgradeServer::on_timerout_callback(struct ev_loop *loop, ev_timer *w, int revents) {
    UpgradeServer::CurMy->on_timeout_cb(loop, w, revents);
}

const char *ProtocolDef::Request = "request";
const char *ProtocolDef::Reply = "reply";
const char *ProtocolDef::Token = "token";
const char *ProtocolDef::Result = "result";

