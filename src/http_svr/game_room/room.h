#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "seats.h"
#include "pokers.h"
#include "poker.pb.h"

#define SEAT_SIZE 7

enum GAME_TYPE
{
    PLAYCARD   = 0,
    COMPARE    = 1,
    DRAW       = 2,
};

enum GAME_STATE
{
    INITROOM   = 0,
    WAITING    = 1,
    GAMEING    = 2,
};

class Room
{
private:
    int Seat(int uid,User* user);
public:
    Room(int room_id);
    ~Room();
    int HandleRequest(PokerGame::msg_package & pkg,DataHandle* handle);
    int InitRoom();
    int Broadcast(PokerGame::msg_package & pkg,DataHandle* handle,int ignore_uid = -1);
    int Join(int uid,User* user);
    int Leave(int uid);
    int JoinState(int uid);
    int DealCards(int num,std::string & out);
    int onReConnect(PokerGame::msg_package & pkg,DataHandle* handle);
private:
    int onDealCards(PokerGame::msg_package & pkg,DataHandle* handle);
    int onStartGame(PokerGame::msg_package & pkg,DataHandle* handle);
    int onLeaveRoom(PokerGame::msg_package & pkg,DataHandle* handle);
    int onPlayCards(PokerGame::msg_package & pkg,DataHandle* handle);
    int onFoldCards(PokerGame::msg_package & pkg,DataHandle* handle);
    int onOverGame(PokerGame::msg_package & pkg,DataHandle* handle);
public:
    int                       _room_id;
    int                       _user_num;
    GAME_STATE                _state;
    Pokers                    _pokers;
    PokerGame::room_conf      _conf;
    std::vector<Seats>        _seats;
    std::string               _comm_cards;
};

