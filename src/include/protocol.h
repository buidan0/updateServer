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
	UInt32 magicValue;  //4字节: 魔数，值是0x79183351用以标识header的开始
	UInt16 version;     //2字节: 协议版本号，目前版本号是0x0001
	UInt16 encryptType; //2字节: 加密类型，0表示不加密（服务器也会返回未加密的响应）, 1-DES加密
	UInt32 xmlLen;      //4字节: 加密报文长度
	UInt16 command;     //2字节: 命令码
	UInt16 reserve1;    //2字节: 保留，为0
	UInt32 reserve2;    //4字节: CRC校验，目前不使用，为0
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
