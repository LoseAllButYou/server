#include <sys/types.h>
#include <sys/socket.h>

#include "http.h"
#include "../loger.h"
#include "stdlib.h"
#include "base64.hpp"
#include "sha1.hpp"
#include "gzip.hpp"


// 200：正确的请求返回正确的结果，如果不想细分正确的请求结果都可以直接返回200。
// 201：表示资源被正确的创建。比如说，我们 POST 用户名、密码正确创建了一个用户就可以返回 201。
// 202：请求是正确的，但是结果正在处理中，这时候客户端可以通过轮询等机制继续请求。
// 203：请求的代理服务器修改了源服务器返回的 200 中的内容，我们通过代理服务器向服务器 A 请求用户信息，服务器 A 正常响应，但代理服务器命中了缓存并返回了自己的缓存内容，这时候它返回 203 告诉我们这部分信息不一定是最新的，我们可以自行判断并处理。
// 300：请求成功，但结果有多种选择。
// 301：请求成功，但是资源被永久转移。比如说，我们下载的东西不在这个地址需要去到新的地址。
// 303：使用 GET 来访问新的地址来获取资源。
// 304：请求的资源并没有被修改过。
// 308：使用原有的地址请求方式来通过新地址获取资源。
// 400：请求出现错误，比如请求头不对等。
// 401：没有提供认证信息。请求的时候没有带上 Token 等。
// 402：为以后需要所保留的状态码。
// 403：请求的资源不允许访问。就是说没有权限。
// 404：请求的内容不存在。
// 406：请求的资源并不符合要求。
// 408：客户端请求超时。
// 413：请求体过大。
// 415：类型不正确。
// 416：请求的区间无效。
// 500：服务器错误。
// 501：请求还没有被实现。
// 502：网关错误。
// 503：服务暂时不可用。服务器正好在更新代码重启。
// 505：请求的 HTTP 版本不支持。
Httpd::Httpd()
    :_ret_code(0),_connect_state(NO_STATE)
{
    _code_str[101] = "WebSocket Protocol Handshake";
    _code_str[200] = "OK";
    _code_str[201] = "OK";
    _code_str[202] = "CREATING";
    _code_str[301] = "MovedPermanently";
    _code_str[303] = "GET_NEW";
    _code_str[400] = "WRONG_REQUEST";
    _code_str[401] = "NOT_SIGNED";
    _code_str[403] = "NO_LIMIT";
    _code_str[404] = "NOT_FIND";
    _code_str[406] = "RES_ERR";
    _code_str[413] = "BIGGER";
}
Httpd::~Httpd()
{
}

int Httpd::RecvRequest(int fd)
{
    LOG_DBG("----------------------------------------");
    _ret_code == 0;
    int ret = 0;
    if( (ret=RecvLine(fd))<0)
    {
    LOG_DBG("----------------------------------------");

        return ret;
    }
    if((ret =RecvHead(fd))<0)
    {
    LOG_DBG("recvhead ret:%d retcode :%d",ret,_ret_code);

        return ret;
    }
    if((ret =RecvData(fd))<0)
    {
    LOG_DBG("recvData ret:%d retcode :%d",ret,_ret_code);

        return ret;
    }
    LOG_DBG("end recv ret:%d retcode :%d",ret,_ret_code);

    return 0;
}

int Httpd::RecvLine(int fd)
{
    int deal_state =0;// 0:method 1:url 2:version
    std::string buf ;
    bool   end = false;
    //先读长度4个字节
    while(true)
    {
        int ret = 0;
        char ch = '\0';
        ret =recv(fd,&ch,1,0);
        if(ret == 0)
        {
            LOG_DBG("fd : %d close!",fd);
            return -1;
        }
        else if(ret<0)
        {
            if(errno == EINTR|| errno == EAGAIN)
            {
                //LOG_DBG("fd : %d close!errno : %d  std %s ",fd,errno,strerror(errno));
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",fd,errno);
                return -2;
            }
        }
        if(!end && ch == '\n')
        {
            _ret_code = 400;
            return 0;
        }
        if(ch == ' ')
        {
            if(deal_state == 0)
            {
                _req_k_v["method"] = buf;
                deal_state++;
            }
            else if(deal_state == 1)
            {
                AnalysisUrl(buf);
                deal_state ++;
            }
            LOG_DBG("http value %s",buf.c_str());
            std::string().swap(buf);
            continue;
        }
        if(deal_state == 2)
        {
            if(ch == '\r')
            {
                _req_k_v["version"] = buf;
                end = true;
                continue;
            }
            if(end)
            {
                if(ch != '\n')
                {
                    _ret_code = 400;
                }
                return 0;
            }        
        }
        if(ch == '\r')
        {
            _ret_code = 400;
            return 0;
        }
        buf.push_back(ch);
    }
}

int Httpd::RecvHead(int fd)
{
    LOG_DBG("----------------------------------------");

    std::string buf  , key ;
    bool   is_key =true;
    bool   end = false;
    bool   end_line= false;
    bool   end_head = false;
    //先读长度4个字节
    while(true)
    {
        int ret = 0;
        char ch = '\0';
        ret =recv(fd,&ch,1,0);
        if(ret == 0)
        {
            LOG_DBG("fd : %d close!",fd);
            return -1;
        }
        else if(ret<0)
        {
            if(errno == EINTR|| errno == EAGAIN)
            {
                //LOG_DBG("fd : %d close!errno : %d  std %s ",fd,errno,strerror(errno));
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",fd,errno);
                return -2;
            }
        }
        
        if(ch == '\n' && !end && !end_head)
        {
            _ret_code = 400;
            return 0;
        }
        if(end)
        {
            if(ch != '\n')
            {
                _ret_code = 400;
                return 0;
            }
            else
            {
                is_key = true;
                end_line = true;
                end = false;
                continue;
            }
            
        }
        if(end_line)
        {
            if(ch == '\r')
            {
                end_head =true;
                continue;
            }
            else
            {
                end_line == false;
            }
            
        }
        if( end_head )
        {
            if(ch!='\n')
            {
                _ret_code = 400;
            }
            return 0;
        }
        if(ch == ':'&& is_key)
        {
            is_key = false;
            if(buf.length() == 0)
            {
                _ret_code = 400;
                return 0;
            }
            _req_k_v[buf] = "";
            key = buf;
            LOG_DBG("head key:%s",buf.c_str());
            std::string().swap(buf);
            continue;
        }
        if(ch == '\r')
        {
            if(end_head)
            {
                continue;
            }
            if(key.length() == 0)
            {
                _ret_code = 400;
                return 0;
            }
            _req_k_v[key] = buf;
            LOG_DBG("head value:%s",buf.c_str());
            std::string().swap(buf);
            end =true;
            continue;
        }
        if(buf.length() == 0 && ch == ' ')
        {
            continue;
        }
        end_line =false;
        end =false;
        buf.push_back(ch);
    }
    return 0;
}

int Httpd::RecvData(int fd)
{
    if(!HaveKey("method"))
    {
        _ret_code = 400;
        return 0;
    }
    std::string  method = GetValueByKey("method");
    if(method == "")
    {
        _ret_code = 400;
        return 0;
    }
    if(method=="GET")
    {  
        return 0;
    }
    else if(method == "POST")
    {
        if(!HaveKey("Content-length"))
        {
            return 0;
        }
        std::string len = GetValueByKey("Content-length");
        if(len.length() == 0 || len == "0")
        {
            return 0;
        }
        int length = atoi( len.c_str() );
        return PostContent(fd,length);
    }
    else
    {
        _ret_code = 405;
    }
    return 0;
}

int Httpd::PostContent(int fd,int length)
{
    char * buf = new char[length];
    if(buf == NULL)
    {
        LOG_ERROR("alloc error!");
        return -1;
    }
    int size = 0;
    bool read_key = true;
    //先读长度4个字节
    while(true)
    {
        int ret = 0;
        char ch = '\0';
        ret =recv(fd,buf,length -size,0);
        if(ret == 0)
        {
            LOG_DBG("fd : %d close!",fd);
            return -2;
        }
        else if(ret<0)
        {
            if(errno == EINTR|| errno == EAGAIN)
            {
                //LOG_DBG("fd : %d close!errno : %d  std %s ",fd,errno,strerror(errno));
                continue;
            }
            else
            {
                LOG_ERROR("read err fd : %d errno : %d",fd,errno);
                return -3;
            }
        }
        size += ret;
        if(size == length)
        {
            break;
        }
    }
    std::string tmp(buf,0,length);
    if(GetValueByKey("Content-Encoding").find("gzip")!=std::string::npos )
    {
       if( Decode(GZIP,tmp,tmp)<0)
       {
           SetRetCode(0);
           return 0;
       }
    }
    AnalysisQuarys(tmp);
    return 0;
}
int Httpd:: AnalysisQuarys(const std::string & quarys)
{
    LOG_DBG("-----%s",quarys.c_str());
    int length = quarys.length();
    if(length == 0)
    {
        return 0;
    }
    int size = 0;
    std::string buf ,key;
    while(size<length)
    {
        char ch = quarys[size];
        if(size == length-1)
        {
            if(key.length() == 0)
            {
                _ret_code = 400;
                return 0;
            }
            buf.push_back(ch);
            LOG_DBG("http value %s",buf.c_str());
            _req_k_v[key] = buf;
            break;
        }
        ++size;
        if(ch == '\n'|| ch =='\r')
        {
            _ret_code = 400;
            return 0;
        }
        if(ch == '=')
        {
            if(buf.length() == 0)
            {
                _ret_code = 400;
                return 0;
            }
            key = buf;
            LOG_DBG("http key %s",buf.c_str());
            std::string().swap(buf);
            continue;
        }
        if(ch == '&')
        {
            _req_k_v[key] = buf;
            LOG_DBG("http value %s",buf.c_str());
            std::string().swap(buf);
            continue;
        }
        buf.push_back(ch);
       
    }
    return 0;
}

int Httpd::AnalysisUrl(const  std::string & quarys)
{
    LOG_DBG("-----------AnalysisUrl");
    if(!HaveKey("method"))
    {
        _ret_code = 400;
        return 0;
    }
    int pos = quarys.find("?");
    if(pos==std::string::npos)
    {
        _req_k_v["url"] = quarys;
        return 0;
    }
    else
    {
        if(GetValueByKey("method")== "POST"||GetValueByKey("method")== "post")
        {
            _req_k_v["method"] = "GET";
        }
        _req_k_v["url"] = std::string(quarys.substr(0,pos));
        
        return AnalysisQuarys(std::string(quarys.substr(pos+1,quarys.length()-pos)));
    }
    return 0;
}

int Httpd::SynthesisResponse(const std::string & headers,const std::string & content ,std::string & out)
{
    out = std::string(HTTP_VERSION)+std::string(" ")+std::to_string(_ret_code)+std::string(" ")+
        _code_str[_ret_code]+"\r\n" +headers +"\r\n";
    if(content.length() != 0 && content!="")
    {
        out = out + content;
    }

    return 0;
}

std::string Httpd::MakeWebSocketKey(const std::string &key)
{
    //SHA1 sha1;
    std::string tmp =key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",a,b;
    int size = 0;
    // if(!sha1.SHA_GO(tmp.c_str(),out))
    // {
    //     return std::string();
    // }
    sha1(tmp,a);
    LOG_DBG("sha1 out %s len:%d",a.c_str(),a.length());
    if(StringToHex(a,b)<0)
    {
        LOG_ERROR("err string to hex");
        return std::string();
    }
    LOG_DBG("sha1 out %s len:%d",b.c_str(),b.length());

    return base64_encode(b.c_str(),b.length());
}

std::string Httpd::GetValueByKey(const std::string &key)
{
    if(HaveKey(key))
    {
        return _req_k_v[key];
    }
    return std::string();
}

bool Httpd:: HaveKey(const std::string &key)
{
    if(_req_k_v.find(key) != _req_k_v.end())
    {
        return true;
    }
    return false;
}

int Httpd::Encode(ENCODE_TYPE encode_type,const std::string & input ,std::string & out)
{
    if(input.size()<2048)
    {
        LOG_DBG("small not gzip!!!");
        return -1;
    }
    if(encode_type == GZIP)
    {
        int size=0;
        char  * buf = new char [input.length()];
         if((size=gzCompress(input.data(),input.size(),buf,input.size()))<0)
         {
             delete buf;
             return -2;
         }
        LOG_DBG("gzip compess srclen:%u len:%lu",input.length(),size);
        std::string(buf,size).swap(out);
        delete buf;
    }
    return 0;
}

int Httpd::Decode(ENCODE_TYPE encode_type,const std::string & input ,std::string & out)
{
    if(encode_type == GZIP)
    {
        char * buf = new char [input.size()];
        int size = 0;
        if((size=gzDecompress(input.data(),input.size(),buf,input.size()))<0)
        {
            delete buf;
            return -2;
        }
        LOG_DBG("---------zip succ");
        out=std::string(buf,size);
        delete buf;
    }
    return 0;
}

void Httpd:: Clear()
{
    std::map<std::string,std::string> tmp;
    tmp.swap(_req_k_v); 
}
