#include "config.h"
#include <fstream>
#include <stdlib.h>
#include <errno.h>
#include "loger.h"

using namespace std;

Config::Config(std::string path )
    :conf_path(path)
{}

Config::~Config()
{
}

void Config::SetPath(std::string path)
{
    if(path!="")
    {
        conf_path = path;
    }
}

int Config::ReadConfValue()
{
    ifstream fin( conf_path.c_str() );  
    string  line;  
    while ( getline(fin,line) )
    {
        static char white_space[] ={" \n\f\t\r\v"};
        int pos = line.find_first_not_of(white_space);
        if(pos==string::npos||line[pos]=='#')
        {
            continue;
        }
        string key = GetKey(line);
        string value = GetValue(line);
        if(key .size() == 0 || value.size() == 0)
        {
            continue;
        }
        conf_map[key]=value;
        LOG_DBG("key = %s value = %s",key.c_str(),value.c_str());
    }
    fin.close();
    return 0;
}

string Config::GetKey(std::string & line)
{
    size_t pos = line.find_first_of("=");
    if(pos == string::npos)
    {
        return "";
    }
    string key;
    for(int i =0;i<pos;++i)
    {
        if(line[i]!=' ')
        {
            key.push_back(line[i]);
        }
    }
    return key;
}

string Config::GetValue(std::string & line)
{
    size_t pos = line.find_first_of("=");
    if(pos == string::npos)
    {
        return "";
    }
    string value,white;
    char tmp = '\0';
    for(int i =pos + 1;i<line.size();++i)
    {
        tmp = line[i];
        if((tmp == ' ' && value.size()>0 ))
        {
            white.push_back(' ');
        }
        if( (tmp != ' '&& tmp != '\n' && tmp != '\t'
        && tmp != '\r'&& tmp != '\f' && tmp != '\v' && tmp != '"' && tmp != '\'') )
        {
            if(white.length()>0)
            {
                value+=white;
                string().swap(white);
            }
            value.push_back(tmp);
        }
    }
    return value;
}

bool Config::IfHaveKey(const string & find_key)
{
    if(conf_map.find(find_key)== conf_map.end())
    {
        return false;
    }
    return true;
}


int Config::GetValue(std::string key,int & out_value)
{
    if(!IfHaveKey(key))
    {
        LOG_ERROR("no key %s",key.c_str());
        return -1;
    }
    const string& string_value = conf_map[key];
    out_value = atoi(string_value.c_str());
    if(errno == EINVAL)
    {
        return -2;
    }
    return 0;
}

int Config::GetValue(std::string key,std::vector<int> & out_value)
{
    if(!IfHaveKey(key))
    {
        LOG_ERROR("no key %s",key.c_str());
        return -1;
    }
    const string& string_value = conf_map[key];
    string value;
    char tmp = '\0';
    for(int i =0;i<string_value.size();++i)
    {
        tmp = string_value[i];
        if( tmp!=','&& tmp!= ' ' )
        {
            value.push_back(tmp);
        }
        if( tmp ==','|| (size_t)i == string_value.size()-1 )
        {
            out_value.push_back(atoi(value.c_str()));
            string().swap(value);
        }
    }
    return 0;
}

int Config::GetValue(std::string key,bool & out_value)
{
    if(!IfHaveKey(key))
    {
        LOG_ERROR("no key %s",key.c_str());
        return -1;
    }
    const string& string_value = conf_map[key];
    out_value = string_value == "TRUE" || string_value == "true";
    return 0;
}

int Config::GetValue(std::string key,double & out_value)
{
    if(!IfHaveKey(key))
    {
        LOG_ERROR("no key %s",key.c_str());
        return -1;
    }
    const string& string_value = conf_map[key];
    out_value = strtod(string_value.c_str(),NULL);
    return 0;
}

int Config::GetValue(std::string key,std::string & out_value)
{
    if(!IfHaveKey(key))
    {
        LOG_ERROR("no key %s",key.c_str());
        return -1;
    }
    out_value = conf_map[key];
    return 0;
}

void Config::IncompleteMatch(const std::string& incomplete_key, std::vector<string> & out_vec)
{
    std::map<std::string,std::string>::iterator it = conf_map.begin();
    std::string key="" ;
    for(;it!= conf_map.end();it++)
    {
        if(it->first.find(incomplete_key.c_str())!= -1)
        {
            std::string tmp;
            int n = it->first.find_first_of("_");
            if(n<0)
            {
                continue;
            }
            tmp = it->first.substr(0,n);
            if(key != tmp)
            {
                key = tmp;
                out_vec.push_back(key);
            }
        }
    }
}
