#include "user.h"
#include "datahandle.h"

User::User(int id)
    :_user_id(id),_online(true),_handle(NULL)
{
}

User::~User()
{
}

int User::AddhandCards(const std::string& cards)
{
    _cards+=cards;
    return 0;
}

int User::PlayCards(const std::string & cards)
{
    _play_cards=cards;
    _cards.erase(remove_if(_cards.begin(), _cards.end(),
        [cards](char ch) { 
            return cards.find(ch)!=std::string::npos; }),
        _cards.end());
    LOG_DBG("_cards length %d",_cards.length());
    return 0;
}

int User::FoldCards(const std::string & cards)
{
    _play_cards.clear();
    _cards.erase(remove_if(_cards.begin(), _cards.end(),
        [cards](char ch) { 
            return cards.find(ch)!=std::string::npos; }),
        _cards.end());
    LOG_DBG("_cards length %d",_cards.length());
    return 0;
}

int User::ClearPlayCards()
{
    std::string().swap(_play_cards);
    return 0;
}

int User::ClearHandCards()
{
    std::string().swap(_cards);
    return 0;
}
