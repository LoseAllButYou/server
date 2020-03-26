#pragma once

#include <iostream>
#include <string>
#include "loger.h"

class DataHandle;

class User
{
private:
    /* data */
public:
    User(int id = -1);
    ~User();
    int AddhandCards(const std::string &cards);
    int PlayCards(const std::string &cards);
    int FoldCards(const std::string &cards);
    int ClearPlayCards();
    int ClearHandCards();
public:
    bool        _online;
    int         _user_id;
    std::string _cards;
    std::string _play_cards;
    DataHandle * _handle;
};