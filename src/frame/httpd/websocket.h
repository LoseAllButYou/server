#pragma once

#include <iostream>
#include <string>
#include <arpa/inet.h>
#include "string.h"

//    0               1               2               3              
//    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
//   +-+-+-+-+-------+-+-------------+-------------------------------+
//   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
//   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
//   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
//   | |1|2|3|       |K|             |                               |
//   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
//   |     Extended payload length continued, if payload len == 127  |
//   + - - - - - - - - - - - - - - - +-------------------------------+
//   |                               |Masking-key, if MASK set to 1  |
//   +-------------------------------+-------------------------------+
//   | Masking-key (continued)       |          Payload Data         |
//   +-------------------------------- - - - - - - - - - - - - - - - +
//   :                     Payload Data continued ...                :
//   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
//   |                     Payload Data continued ...                |
//   +---------------------------------------------------------------+

#define UCHAR  unsigned char 
#define INT64  long long 
#define UINT32 unsigned int
#define MAX_WS_HEADER 12
#define MIN_WS_HEADER 2
#define MAX_INT16_VALUE 0xffff
enum WS_Status
{
    WS_STATUS_CONNECT = 0,
    WS_STATUS_UNCONNECT = 1,
};
enum WS_FrameType
{
    WS_MID_FRAME     = 0x00,
    WS_ERROR_FRAME   = 0x0B,
    WS_TEXT_FRAME    = 0x01,
    WS_BINARY_FRAME  = 0x02,
    WS_PING_FRAME    = 0x09,
    WS_PONG_FRAME    = 0x0A,
    WS_CLOSING_FRAME = 0x08,
};

//分片状态
enum WS_DataState
{
    WS_OVER        = 0 ,
    WS_CONTINUE    = 1 ,
};

class WebSocket
{
public:
    WebSocket();
    ~WebSocket();
    int ReadWebSocket(int fd);
    int  EncodeFream(std::string& in_out,WS_FrameType fream_type,UINT32 mask=0);  
    const char*  GetReadBuf(INT64& size);
    int   AddWriteBuf(std::string& input);
private:
    int AnalysisFreamHead(const char headers[2],bool & mask ,bool & final,UCHAR & op_code,char& data_len);
    int RecvToBuf(int fd,const void* buf,INT64 read_len);
    template<class T>
    bool MaskOrUnMaskFream(T data,INT64 length,UINT32 mask);

public:
    WS_DataState _read_state;
    WS_DataState _write_state;
    WS_FrameType _frame_type;
private:
    char         _mask[4];
    char *       _read_buf;
    INT64        _read_buf_size;
    INT64        _read_size;
    char              _rsvs;
};