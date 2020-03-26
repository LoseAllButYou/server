#include <sys/types.h>
#include <sys/socket.h>

#include "websocket.h"
#include "loger.h"
//   +0---------------1---------------2---------------3--------------+
//   |1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8|
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

WebSocket::WebSocket()
    :_read_state(WS_OVER),_write_state(WS_OVER),_frame_type(WS_TEXT_FRAME),_mask({0,0,0,0})
    ,_read_buf(NULL),_read_buf_size(0),_read_size(0),_rsvs(0)
{}
WebSocket::~WebSocket()
{
    if(_read_buf)
    {
        delete _read_buf;
    }
}

int WebSocket::AnalysisFreamHead(const char headers[2],bool & mask ,bool & final,UCHAR & op_code,char& data_len)
{
    final = headers[0]&0x80==0?false:true;
    mask = headers[1]&0x80==0?false:true;
    _rsvs = headers[0]&0x70;
    op_code = headers[0]&0x0f;
    data_len = (char)(headers[1]&0x7f);
    return 0;
}

int WebSocket::RecvToBuf(int fd,const void* buf,INT64 read_len)
{
    INT64 size = 0;
    while(true)
    {
        int ret=0;
        ret =recv(fd,((char*)buf)+size,read_len-size,0);
        if(ret == 0)
        {
            return -1;
        }
        else if(ret<0)
        {
            if(errno == EINTR||errno == EAGAIN)
            {
                //LOG_DBG("_fd : %d close!errno : %d  std %s ",fd,errno,strerror(errno));
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",fd,errno);
                return -2;
            }
        }
        LOG_DBG("++++++++++++++read size:%d need:%lld",ret,read_len);
        size+=(INT64)ret;
        if(size == read_len)
            break;
    }
    return 0;
}


int WebSocket::ReadWebSocket(int fd)
{
    INT64 int64_data_length = 0;
    short   int16_data_length = 0;
    bool mask ,final;
    UCHAR  op_code;
    char  fream_len;
    char headers[2]={'\0'};
    //读2字节
    int ret = 0;
    if((ret=RecvToBuf(fd,headers,2))<0)
    {
        LOG_DBG("recv headers err! errno : %d",errno);
        return -1;
    }
    if(0>AnalysisFreamHead(headers,mask,final,op_code,fream_len))
    {
        return -1;
    }
    _read_state = final? WS_OVER:WS_CONTINUE;
    _frame_type = (WS_FrameType)op_code;

    if(fream_len==126)
    {
        LOG_DBG("+++++++++++++++++++++++ read short");
        if((ret=RecvToBuf(fd,&int16_data_length,sizeof(short)))<0)
        {
            LOG_DBG("recv headers err int32! errno : %d",errno);
            return -1;
        }
        int16_data_length =ntohs(int16_data_length);

    }
    else if(fream_len==127)
    {
        LOG_DBG("+++++++++++++++++++++++ read int64");

        if((ret=RecvToBuf(fd,&int64_data_length,sizeof(INT64)))<0)
        {
            LOG_DBG("recv headers err INT64! errno : %d",errno);
            return -1;
        }
        int64_data_length =ntohl(int16_data_length);
    }
    if(mask)
    {
        if((ret=RecvToBuf(fd,_mask,4))<0)
        {
            LOG_DBG("recv headers err mask! errno : %d",errno);
            return -1;
        }
    }
    if(fream_len == 0)
    {
        return 0;
    }
    int64_data_length = (INT64)(fream_len<126?fream_len:fream_len==126?int16_data_length:int64_data_length);
    if(_read_buf_size<int64_data_length)
    {
        if(_read_buf != NULL)
        {
            delete _read_buf;
            _read_buf = NULL;
        }
        _read_buf = new char[int64_data_length+1];
        if(_read_buf==NULL)
        {
            LOG_DBG("alloc _read_buff err!");
            return -1;
        }
        _read_buf_size = int64_data_length+1;
        _read_size = int64_data_length;
    }
    else
    {
        _read_size = int64_data_length;
    }
    memset(_read_buf,0,_read_buf_size);
    if((ret=RecvToBuf(fd,_read_buf,int64_data_length))<0)
    {
        LOG_DBG("recv headers err data! errno : %d",errno);
        return -1;
    }
    
    if(mask)
    {
        MaskOrUnMaskFream(_read_buf,int64_data_length,(*(UINT32*)_mask));
    }
    LOG_DBG("mask:%d,final:%d,op_code:%d,fream_len%d buf %s",mask,final,op_code,int64_data_length,_read_buf);
    return 0;
}

int WebSocket:: EncodeFream(std::string& in_out,WS_FrameType fream_type,UINT32 mask)
{
    char buf[MAX_WS_HEADER] ={0};
    int header_size = MIN_WS_HEADER+(mask==0?0:4)+(in_out.length()<126?0:in_out.length()==126?2:8);
    LOG_DBG("-+-+-+-+-+head size %d in out len : %d",header_size,in_out.length());
    buf[0] =0x80 + fream_type;
    if(mask!=0)
    {
        buf[1] = 0x80;
    }
    if(in_out.length()<126)
    {
        LOG_DBG("111111111111111111111111111111");
        buf[1] |=  (char)in_out.length();
        if(mask!=0)
        {
            *(UINT32*)(buf+2) = mask;
            LOG_DBG("mask_____ 0x%x",*(UINT32*)(buf+2));
        }
    }
    else if(in_out.length()<MAX_INT16_VALUE)
    {
        buf[1] |= 126;
        *(short*)(buf+2) = (short)in_out.length();
        if(mask!=0)
        {
            *(UINT32*)(buf+4)  = mask;
        }
    }
    else
    {
        buf[1] |= 127;
        *(INT64*)(buf+2) = (INT64)in_out.length();
        if(mask!=0)
        {
            *(UINT32*)(buf+10)  = mask;
        }
    }
    if(mask!=0)
    {LOG_DBG("+++++++++++++++++++++++++++++++++");
        MaskOrUnMaskFream(in_out,in_out.length(),mask);
    }
    in_out = std::string(buf,0,header_size)+in_out;

    // for(int i = 0;i<header_size;++i)
    // {
    //     LOG_DBG("i:%d  0x%x",i,(UCHAR)buf[i]);
    // }
    // for(int i = 0;i<in_out.length();++i)
    // {
    //     LOG_DBG("i:%d  0x%d",i,(UCHAR)in_out[i]);
    //     if(header_size-1 == i)
    //     {
    //         LOG_DBG("+++++++++++++++++++++++++++++++++");
    //     }
    // }
    return 0;
}

const char*  WebSocket:: GetReadBuf(INT64& size)
{
    size = _read_size;
    _read_size = 0;
    return size==0?NULL:_read_buf;
}

int   WebSocket:: AddWriteBuf(std::string& input)
{
    return 0;
}

template<class T>
bool WebSocket:: MaskOrUnMaskFream(T data,INT64 length,UINT32 mask)
{
    for(INT64 i =0 ;i<length;++i)
    {
        data[i]=data[i]^((char*)&mask)[i%4];
    }
    return true;
}