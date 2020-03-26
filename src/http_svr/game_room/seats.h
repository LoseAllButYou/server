#pragma once

#include <iostream>
#include <string>
#include "user.h"
#include "loger.h"

class Seats
{
private:
    /* data */
public:
    Seats(int seat_id);
    ~Seats();
    int AddUser(int uid,User*   user);
    int RemoveUser();
public:
    int     _seat_id;
    int     _uid;
    User*   _user;
};