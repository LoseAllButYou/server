#pragma once

#include <iostream>
#include "string.h"
#include <string>
#include <map>

#define HTTP_VERSION "http/1.1"

enum ENCODE_TYPE
{
    NOENCODE = 0,
    GZIP     = 1,
};

enum CONN_STATE
{
    NO_STATE = 0,
    MOVED    = 1,
};

class Httpd
{
private:
    /* data */
public:
    Httpd(/* args */);
    ~Httpd();
    int RecvRequest(int fd);
    int PostContent(int fd,int length);
    int AnalysisQuarys(const std::string & quarys);
    int AnalysisUrl(const std::string & quarys);
    int SynthesisResponse(const std::string & headers,const std::string & content ,std::string & out );
    std::string& SynthesisHeaders();
    std::string GetValueByKey(const std::string &key);
    bool HaveKey(const std::string &key);
    int RetCode(){return _ret_code;};
    void SetRetCode (int code){_ret_code = code;}
    int Encode(ENCODE_TYPE encode_type,const std::string & input ,std::string & out);
    int Decode(ENCODE_TYPE encode_type,const std::string & input ,std::string & out);
    std::string MakeWebSocketKey(const std::string &key);
    void Clear();
    void SetState(CONN_STATE state){ _connect_state = state;}
    CONN_STATE GetState(){return _connect_state;}
private:
    int RecvLine(int fd);
    int RecvHead(int fd);
    int RecvData(int fd);
public:

private:
    std::map < int ,std::string >       _code_str;
    std::map < std::string,std::string >    _req_k_v;
    CONN_STATE                                 _connect_state;
    int          _ret_code;
};


