#pragma once
#include <iostream>

#include <string>
#include <string.h>
#include <map>
#include <vector>

class Config
{
private:
    /* data */
    std::string GetKey(std::string & line);
    std::string GetValue(std::string & line);
public:
    Config(std::string path = "./config.ini");
    ~Config();
    int GetValue(std::string key,int & out_value);
    int GetValue(std::string key,std::vector<int> & out_value);
    int GetValue(std::string key,bool & out_value);
    int GetValue(std::string key,double & out_value);
    int GetValue(std::string key,std::string & out_value);
    bool IfHaveKey(const std::string & find_key);
    void SetPath(std::string path);
    void IncompleteMatch(const std::string& incomplete_key,std::vector<std::string> & out_vec);
    int ReadConfValue();
    int GetAll(std::map<std::string,std::string> & out){ out=conf_map; return 0;}
public:
    std::string  conf_path;
private:
    std::map<std::string,std::string> conf_map;
};

