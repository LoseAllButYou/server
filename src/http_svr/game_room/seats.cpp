#include "seats.h"

Seats::Seats(int seat_id)
    :_seat_id(seat_id),_uid(-1),_user(NULL)
{}

Seats::~Seats()
{
}

int Seats::AddUser(int uid,User* user)
{
    if(!_user)
    {
        _uid=uid;
        _user = user;
    }
    else
    {
        LOG_DBG("user exist! id:%d",_uid);
        return -1;
    }
    
    return 0;
}

int Seats::RemoveUser()
{
    if(_user)
        delete _user;
    _user = NULL;
    _uid = 0;
    return 0;
}
