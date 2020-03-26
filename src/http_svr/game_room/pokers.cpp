#include <pokers.h>
#include "loger.h"
#include "timer.h"
Pokers::Pokers(int pokers_num )
    :_have_joker(true)
{
}

Pokers::~Pokers()
{
}

void Pokers::Shuffle()
{
    int len =_game_pokers.length()/2;
    for(int i =0;i<len;++i)
    {
        timeval now ;
        gettimeofday(&now,NULL);
        srand(now.tv_usec);
        int rand_num =rand()% (_game_pokers.length()-len)+ len;
        char ch = _game_pokers[i];
        _game_pokers[i] = _game_pokers[rand_num];
        _game_pokers[rand_num] =ch;
    }
            LOG_DBG("%s",_game_pokers.c_str());

}

void Pokers::ReSet(int pokers_num)
{
    _game_pokers.clear();
    for(int i = 0;i<pokers_num;++i)
    {
        if(_have_joker)
            _game_pokers += g_src_pokers;
        else
        {
            std::string pokers(g_src_pokers,0,MAX_POKER_NUM);
            LOG_DBG("%s",pokers.c_str());
            _game_pokers.append( pokers.substr(2));
            LOG_DBG("%s",_game_pokers.c_str());
        }
    }
}

int Pokers::Deal(int num,std::string & out)
{
    if(num<=0||_game_pokers.length()<num)
        return -1;
    out = _game_pokers.substr(0,num);
    _game_pokers = _game_pokers.substr(num,_game_pokers.length()-num);
    return 0;
}
