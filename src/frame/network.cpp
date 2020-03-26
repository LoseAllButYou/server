#include "network.h"
#include "loger.h"
#include "thread_pool/define.h"
#include "thread_pool/thread_pool.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <cstring>
using namespace std;

void * NetworkThreadFunc(void* args)
{
    G_NETWORK->Run(NULL);
    return args;
}

Network* Network::g_self = NULL;

static pthread_mutex_t m_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m_data_mutex = PTHREAD_MUTEX_INITIALIZER;

//设置非阻塞fd
int Network::SetNonBlock(int fd)
{
	int old_flag=fcntl(fd,F_GETFL);
	if(old_flag<0||fcntl(fd,F_SETFL,old_flag|O_NONBLOCK)<0)
    {
        LOG_ERROR("set non block err!");
        close(fd);
        return -1;
    }
	return 0;
}


Network::Network()
    :Task(TASK_NETWORK,"network")
    ,_ip("") ,_port (0)
    ,epoll_fd(0),listen_fd(0),epoll_close(false)
{
}

Network::~Network()
{
    pthread_mutex_destroy(&m_data_mutex);
    pthread_mutex_destroy(&m_data_mutex);
    std::map<int,DataHandle*>::iterator it= _fd_map.begin();
    for (it=_fd_map.begin();it!=_fd_map.end();)
    {
        it=_fd_map.erase(it);
    }
}

void Network::SetListenHost(std::string ip,short port)
{
    _ip = ip;
    _port = port;
}

void Network::Run(void* args)
{
    StartUp();
}

void Network::Debug()
{

}

int Network::Listen()
{
    if(_ip == "" or _port == 0)
    {
        LOG_ERROR("need init ip and port!!!");
        return -1;
    }
    listen_fd=socket(AF_INET,SOCK_STREAM,0);
    if(listen_fd <0)
    {
        LOG_ERROR("socket err %d",listen_fd);
        return -2;
    }

	struct sockaddr_in server;
	server.sin_family=AF_INET;
	server.sin_addr.s_addr=inet_addr(_ip.c_str());
	server.sin_port=htons((short)_port);
    LOG_DBG("%s,%d",_ip.c_str(),_port);

	if(bind(listen_fd,(struct sockaddr *)&server,sizeof(server))<0){
        LOG_ERROR("bind err %d ,%s",errno,strerror(errno));
        close(listen_fd);
        return -3;
	}

	if(listen(listen_fd,100000)<0){
        LOG_ERROR("lisen err %d ,%s",errno,strerror(errno));
        close(listen_fd);
        return -4;
	}
    // struct timeval timeout = {0,1000};
    // if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval)) != 0)
    // {
    // 	LOG_ERROR("set accept timeout  err %d ,%s",errno,strerror(errno));
    //     close(listen_fd);
    //     return -5;
    // }

    if(SetNonBlock(listen_fd)<0)
    {
        return -6;
    }
    epoll_event ev;
	ev.data.fd=listen_fd;
	//ev.events=EPOLLIN|EPOLLET;
	ev.events=EPOLLIN;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&ev)<0){
		LOG_ERROR("epoll_ctl err %d ,%s",errno,strerror(errno));
		return -7;
	}
    return 0;
}

int Network::Connect(string ip,short port)
{
    ///定义sockfd
    int sock_cli = socket(AF_INET,SOCK_STREAM, 0);
    ///定义sockaddr_in
    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);  ///服务器端口
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());  ///服务器ip

    //连接服务器，成功返回0，错误返回-1
    if (connect(sock_cli, (sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        LOG_ERROR("connect err! ip:%s port:%d",ip.c_str(),port);
        return -1;
    }
    LOG_DBG("connect succ! ip:%s port:%d",ip.c_str(),port);

    if(SetNonBlock(sock_cli)<0)
    {
        return -2;
    }
    epoll_event ev;
	ev.data.fd=sock_cli;
	ev.events=EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,sock_cli,&ev)<0){
		LOG_ERROR("epoll_ctl err %d ,%s",errno,strerror(errno));
		return -3;
	}
    AddFd(sock_cli);
    return sock_cli;
}


int Network::EpollUp()
{
    epoll_fd=epoll_create(1024*10);
	if(epoll_fd<0){
		LOG_ERROR("epoll_create err %d ,%s",errno,strerror(errno));
		return -1;
	}
    return 0;
}

bool Network::AddSocketFd()
{
    pthread_mutex_lock(&m_data_mutex);

    int new_sock=-1;
	struct sockaddr_in client;
	socklen_t len=sizeof(client);
	if((new_sock=accept(listen_fd,(struct sockaddr*)&client,&len))<0){
		LOG_ERROR("accept new socket err! %d ,%s",errno,strerror(errno));
        pthread_mutex_unlock(&m_data_mutex);
        return false;
	}
    if(SetNonBlock(new_sock)<0)
    {
        close(new_sock);
        pthread_mutex_unlock(&m_data_mutex);
        return false;
    }
	epoll_event ev;
	ev.data.fd=new_sock;
	ev.events=EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;

	if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,new_sock,&ev)<0){
        close(new_sock);
		LOG_ERROR("add event to epoll err! fd:%d",new_sock);
        pthread_mutex_unlock(&m_data_mutex);
		return false;
	}
    AddFd(new_sock);
    pthread_mutex_unlock(&m_data_mutex);
    LOG_SHOW("add event to epoll succ! fd:%d",new_sock);
    return true;
}

void Network::DelSocketFd(int fd)
{
    pthread_mutex_lock(&m_data_mutex);
    DelFd(fd);
	if(epoll_ctl(epoll_fd,EPOLL_CTL_DEL,fd,NULL)<0){
		LOG_ERROR("del event to epoll err! fd:%d errno:%d",fd,errno);
	}
    close(fd);
    pthread_mutex_unlock(&m_data_mutex);
    LOG_SHOW("del event to epoll succ! fd:%d",fd);
}


void Network::EpollLoop()
{
    epoll_event event[1024*10];
    int ret_num = 0;
    int timeout=0;
    while(!epoll_close)
    {
        switch(ret_num=epoll_wait(epoll_fd,event,1024*10,timeout))
        {
	        case -1:
            {
	        	LOG_DBG("epoll_wait err! erron :%d %s",errno,strerror(errno));
	        	break;
	        }
            case 0:
            {
                timeout=2000;
                LOG_DBG("need time out!!!!!!! %d",atomic_load(& G_Thread_pool->work_num));
                break;
            }
            default:
            {
                LOG_DBG("ret_num %d",ret_num);
	        	for(int i=0;i<ret_num;++i)
                {
	        		if(event[i].data.fd==listen_fd && event[i].events&EPOLLIN)
                    {
 
                        int count = 0;
	        			while(AddSocketFd())
                        {
                            LOG_DBG("epoll new fd count :%d all : %d",++count,_fd_map.size());
                        }
	        		}
	        		else 
                    {
                        int fd = event[i].data.fd;
                        bool need_add_task=false;
                        if(!HaveFd(fd))
                        {
                            LOG_ERROR("not connect FD : %d ",fd);
                            continue;
                        }
                        if(event[i].events&EPOLLRDHUP)
                        {
                            LOG_DBG("fd : %d close",event[i].data.fd);
                            if(GetHandle(fd)&&GetHandle(fd)->GetTaskID()!=TASK_DATA_CLOSE)
                            {
                                SetEvent(fd,DATA_CLOSE);
                                ClearDataHandle(fd);
                            }
                        }
                        else
                        {
                            if(event[i].events&EPOLLOUT)
                            {
                                LOG_DBG("write event fd : %d",fd);
                                if(!(GetEvent(fd)&DATA_WRITE)&&!(GetEvent(fd)&DATA_CLOSE));
                                {
                                    SetEvent(fd,DATA_WRITE);
                                }
                                if(GetHandle(fd)&&!(GetEvent(fd)&DATA_CLOSE)&&GetHandle(fd)->NeedWrite())
                                {
                                    need_add_task = true;
                                }                        
	        			    }
	        			    if(event[i].events&EPOLLIN)
                            {
                                //LOG_DBG("read event %d ,%d",GetEvent(fd),GetEvent(fd)&DATA_READ);
                                if(!(GetEvent(fd)&DATA_CLOSE))
                                {
                                    SetEvent(fd,DATA_READ);
                                    need_add_task = true;
                                }
                            }
                        }

                        if(need_add_task&&GetEvent(fd)<8)
                        {
                            if(GetHandle(fd))
                            {
                                ++(GetHandle(fd)->task_num);
                                GetHandle(fd)->AddToTreadPool();
                            }
                        }
	        		}
	        	}
	        	break;
		    }  
        }
    }
}

void Network::ClearDataHandle(int fd)
{
    if(GetHandle(fd))
    {
        LOG_DBG("clear handle by fd : %d",fd);
        ++(GetHandle(fd)->task_num);
        GetHandle(fd)->SetTaskClose();
        GetHandle(fd)->AddToTreadPool();
    }
}

int Network::StartUp()
{
    EpollLoop();
    LOG_DBG("epoll loop exit!!!!");
    return 0;
}

DataHandle* Network::GetHandle(int fd)
{
    pthread_mutex_lock(&m_data_mutex);

    if(_fd_map.find(fd)!=_fd_map.end())
    {
        pthread_mutex_unlock(&m_data_mutex);
        std::atomic_thread_fence(memory_order_seq_cst);
        return _fd_map[fd];
    }
    pthread_mutex_unlock(&m_data_mutex);
    std::atomic_thread_fence(memory_order_seq_cst);
    return NULL;
}


int Network::AddFd(int fd)
{
    if(_fd_map.find(fd)!=_fd_map.end())
    {
        LOG_DBG("this FD already exist! fd:%d",fd);
        _fd_map[fd]->Clear();
        //或者处理业务逻辑
    }
    _fd_map[fd]= new DataHandle(fd);
    if(_fd_map[fd]==NULL)
    {
        return -1;
    }
    _fd_map[fd]->SetEvent(DATA_CONNECT);
    return 0;
}

bool Network::DelFd(int fd)
{
    std::map<int,DataHandle*>::iterator it= _fd_map.find(fd);
    if (it!=_fd_map.end())
    {
        delete it->second;
        _fd_map.erase(it);
    }
    return true;
}

bool Network::SetEvent(int fd,int event)
{
    pthread_mutex_lock(&m_data_mutex);
    if(_fd_map.find(fd)==_fd_map.end())
    {
        pthread_mutex_unlock(&m_data_mutex);
        return false;
    }
    _fd_map[fd]->SetEvent(event);
    pthread_mutex_unlock(&m_data_mutex);
    return true;
}

bool Network::DelEvent(int fd,int event)
{
    pthread_mutex_lock(&m_data_mutex);

    if(_fd_map.find(fd)==_fd_map.end())
    {
        pthread_mutex_unlock(&m_data_mutex);
        return false;
    }
    _fd_map[fd]->DelEvent(event);
    pthread_mutex_unlock(&m_data_mutex);

    return true;
}

bool Network::HaveFd(int fd)
{
    pthread_mutex_lock(&m_data_mutex);

    bool ret = _fd_map.find(fd)!=_fd_map.end();
    pthread_mutex_unlock(&m_data_mutex);

    return ret;
}

bool Network::NeedWrite(int fd)
{
    pthread_mutex_lock(&m_data_mutex);
    if(_fd_map.find(fd)==_fd_map.end())
    {
        pthread_mutex_unlock(&m_data_mutex);
        return false;
    }
    pthread_mutex_unlock(&m_data_mutex);
    return _fd_map[fd]->NeedWrite();
}

int Network::GetEvent(int fd)
{
    pthread_mutex_lock(&m_data_mutex);

    if(_fd_map.find(fd)==_fd_map.end())
    {
        pthread_mutex_unlock(&m_data_mutex);
        return -1;
    }
    pthread_mutex_unlock(&m_data_mutex);

    return _fd_map[fd]->Event();
}



Network* Network:: GetInstance()
{
    if(g_self==NULL)
    {
        pthread_mutex_lock(&m_mutex);
        if(g_self==NULL)
        {
            g_self = new Network();
        }
        pthread_mutex_unlock(&m_mutex);
    }
    return g_self;
}