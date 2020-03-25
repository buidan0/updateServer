#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <iostream>

using namespace std;

#define UInt32 unsigned int
#define UInt16 unsigned short

enum
{
	OK = 0, FAIL = 1
};

enum CommandId
{
	LoginRequest = 0x0001,
	LoginReply = 0x9001,
	LogoutRequest = 0x0002,
	LogoutReply = 0x9002,
	HeartBeatRequst = 0x0003,
	UploadRequest = 0x0004,
	UploadReply = 0x9004,
	UploadDone = 0x0005
};

typedef struct MessageHeader
{
	UInt32 magicValue;  //4�ֽ�: ħ����ֵ��0x79183351���Ա�ʶheader�Ŀ�ʼ
	UInt16 version;     //2�ֽ�: Э��汾�ţ�Ŀǰ�汾����0x0001
	UInt16 encryptType; //2�ֽ�: �������ͣ�0��ʾ�����ܣ�������Ҳ�᷵��δ���ܵ���Ӧ��, 1-DES����
	UInt32 xmlLen;      //4�ֽ�: ���ܱ��ĳ���
	UInt16 command;     //2�ֽ�: ������
	UInt16 reserve1;    //2�ֽ�: ������Ϊ0
	UInt32 reserve2;    //4�ֽ�: CRCУ�飬Ŀǰ��ʹ�ã�Ϊ0
} MessageHeader;

class ProtocolDef
{
public:
	static const char* Request;
	static const char* Reply;
	static const char* Token;
	static const char* Result;
};

#endif
