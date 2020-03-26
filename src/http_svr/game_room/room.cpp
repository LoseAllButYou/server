#include "room.h"
#include "websocket.h"
#include "datahandle.h"

Room::Room(int room_id)
    :_room_id(room_id),_user_num(0)
{}

Room::~Room()
{
}

int Room::HandleRequest(PokerGame::msg_package & pkg,DataHandle* handle)
{
    switch (pkg.id())
    {
    case 1:
        {
            break;
        }
    case 4:
        {
            break;
        }
    case 10:
        {
            onStartGame(pkg,handle);
            break;
        }
    case 12:
        {
            onOverGame(pkg,handle);
            break;
        }
    case 15:
        {
            onPlayCards(pkg,handle);
            break;
        }
    case 17:
        {
            onFoldCards(pkg,handle);
            break;
        }       
         
    default:
        return -1;
        break;
    }
    return 0;
}

int Room::onDealCards(PokerGame::msg_package & pkg,DataHandle* handle)
{
    return 0;
}

int Room::onStartGame(PokerGame::msg_package & pkg,DataHandle* handle)
{
    PokerGame::response* res_pkg=new PokerGame::response ;
    PokerGame::deal_cards deal_cards ;
    pkg.set_id( PokerGame::RES_START_GAME);
    if(_user_num == _conf.players_num()&&_state!=GAMEING)
    {
        _state=GAMEING;
        int num = 0;
        if(_conf.deal_num()==-1)
        {
            num=_pokers._game_pokers.length()/_user_num;
        }
        else
        {
            num=_conf.deal_num();
        }
        if(_user_num*num > _pokers._game_pokers.length())
        {
            res_pkg->set_reson("deal cards err");
            res_pkg->set_ret(PokerGame::ERR_REQUEST);
            _state=WAITING;
        }
        else
        {
            _pokers.Shuffle();

            for(int i =1;i<_seats.size();++i)
            {
                if(_seats[i]._user == NULL || _seats[i]._uid <=0)
                {
                    continue;
                }
                std::string cards ;
                if(DealCards(num,cards)<0)
                {
                    delete res_pkg;
                    LOG_DBG("deal cards err");
                    _state=WAITING;
                    return -1;
                }
                LOG_DBG("deal num %d user %d--- cards %s",num,_seats[i]._uid,cards.c_str());
                _seats[i]._user->AddhandCards(cards);
                if(res_pkg == NULL)
                {
                    res_pkg = new PokerGame::response;
                    if(res_pkg == NULL)
                    {
                        return -1;
                        _state=WAITING;
                    }
                }
                res_pkg->set_ret(PokerGame::SUCCESS);
                pkg.set_user_id(_seats[i]._uid);
                std::string res_cards,req_str;
                deal_cards.set_cards(cards);
                deal_cards.SerializeToString(&res_cards);
                res_pkg->set_pkg(res_cards);
                pkg.set_allocated_res_pkg(res_pkg);
                pkg.SerializeToString(&req_str);
                handle->GetWebSocket()->EncodeFream(req_str,WS_BINARY_FRAME);
                _seats[i]._user->_handle->SendPackage(req_str);
                LOG_DBG("{\n%s}", pkg.DebugString().c_str());
                pkg.clear_res_pkg();
                res_pkg = NULL;
            }
        }
    }
    else
    {
        res_pkg->set_reson("players num not full");
        res_pkg->set_ret(PokerGame::ERR_REQUEST);
    }
    if(res_pkg)
    {
        std::string req_str;
        pkg.set_allocated_res_pkg(res_pkg);
        pkg.SerializeToString(&req_str);
        handle->GetWebSocket()->EncodeFream(req_str,WS_BINARY_FRAME);
        handle->SendPackage(req_str);
    }
    return 0;
}

int Room::onOverGame(PokerGame::msg_package & pkg,DataHandle* handle)
{
    PokerGame::response* res_pkg=new PokerGame::response ;
    pkg.set_id( PokerGame::RES_OVER_GAME);
    if(_state == GAMEING)
    {
        _pokers.ReSet(_conf.pokers_num());
        _pokers.Shuffle();
        std::string().swap(_comm_cards);
        for(int i=1;i<_seats.size();++i)
        {
            _seats[i]._user->ClearPlayCards();
            _seats[i]._user->ClearHandCards();
        }
        _state = WAITING;
        res_pkg->set_ret(PokerGame::SUCCESS);
    }
    else
    {
        res_pkg->set_ret(PokerGame::ERR_REQUEST);
        res_pkg->set_reson("not start game");
    }
    
    pkg.set_allocated_res_pkg(res_pkg);
    LOG_DBG("{\n%s}", pkg.DebugString().c_str());

    Broadcast(pkg,handle);
    return 0;
}

int Room::DealCards(int num,std::string & out)
{
    return _pokers.Deal(num,out);
}

int Room::onLeaveRoom(PokerGame::msg_package & pkg,DataHandle* handle)
{
    return 0;
}

int Room::onPlayCards(PokerGame::msg_package & pkg,DataHandle* handle)
{
    PokerGame::response* res_pkg=new PokerGame::response ;
    PokerGame::play_cards play_cards ;
    pkg.set_id( PokerGame::RES_PLAY_CARDS);
    play_cards.ParseFromString(pkg.req_pkg());

    for(int i = 1;i<_seats.size();++i)
    {
        if(_seats[i]._uid==pkg.user_id())
        {
            _seats[i]._user->PlayCards(play_cards.cards());
        }
    }
    res_pkg->set_ret( PokerGame::SUCCESS);
    pkg.set_allocated_res_pkg(res_pkg);
    Broadcast(pkg,handle);
    return 0;
}

int Room::onFoldCards(PokerGame::msg_package & pkg,DataHandle* handle)
{
    PokerGame::response* res_pkg=new PokerGame::response ;
    PokerGame::play_cards play_cards ;
    pkg.set_id( PokerGame::RES_FOLD_CARDS);
    play_cards.ParseFromString(pkg.req_pkg());

    for(int i = 1;i<_seats.size();++i)
    {
        if(_seats[i]._uid==pkg.user_id())
        {
            _seats[i]._user->FoldCards(play_cards.cards());
        }
    }
    res_pkg->set_ret( PokerGame::SUCCESS);
    pkg.set_allocated_res_pkg(res_pkg);
    Broadcast(pkg,handle);
    return 0;
}

int Room::onReConnect(PokerGame::msg_package & pkg,DataHandle* handle)
{
    return 0;
}

int Room::InitRoom()
{

    _pokers._have_joker = _conf.joker();
    if(_conf.pokers_num() <1||_conf.pokers_num()>3)
    {
        LOG_ERROR("conf pokers num err! :%d",_conf.pokers_num());
        return -1;
    }
    else
    {
        _pokers.ReSet(_conf.pokers_num());
        _pokers.Shuffle();
    }
    
    if(_conf.players_num() <2||_conf.players_num()>6)
    {
        LOG_ERROR("conf players num err! :%d",_conf.players_num());
        return -1;
    }
    else
    {
        for(int i=0;i<=_conf.players_num();++i)
        {
            _seats.push_back(Seats(i));
        }
    }
    return 0;
}

int Room::Broadcast(PokerGame::msg_package & pkg,DataHandle* handle,int ignore_uid)
{
    if(handle == NULL)
    {
        return -1;
    }
    std::string req_str;
    pkg.SerializeToString(&req_str);
    handle->GetWebSocket()->EncodeFream(req_str,WS_BINARY_FRAME);
    for(int i =1;i<_seats.size();++i)
    {
        if(_seats[i]._uid != ignore_uid )
        {
            LOG_DBG("seat uid : %d uid : %d fd:%d",_seats[i]._uid,ignore_uid,_seats[i]._user->_handle->GetFD());
            _seats[i]._user->_handle->SendPackage(req_str);
        }
    }
    return 0;
}

int Room::Join(int uid,User* user)
{
    if(_user_num>=_conf.players_num())
    {
        LOG_ERROR("user num biger!");
        return -1;
    }
    return Seat(uid,user);
}
int Room::Seat(int uid,User* user)
{
    for(int i=1;i<_seats.size();++i)
    {
        if(_seats[i]._user == NULL)
        {
            _seats[i]._seat_id = i;
            _seats[i].AddUser(uid,user);
            ++_user_num;
            return 0;
        }
    }
    return -1;
}

int Room::Leave(int uid)
{
    for(int i=1;i<_seats.size();++i)
    {
        if(_seats[i]._user != NULL&&_seats[i]._uid == uid)
        {
            _seats[i].RemoveUser();
            --_user_num;
            return 0;
        }
    }
    return -1;
}

int Room::JoinState(int uid)
{
    for(int i=1;i<_seats.size();++i)
    {
        if(_seats[i]._uid == uid )
        {
            if(_seats[i]._user->_online)
                return -1;
            else
            {
                return 1;
            }
            
        }
    }
    return 0;
}
