#ifndef LOAN_MANAGER_H
#define LOAN_MANAGER_H 

#include <iostream>
#include <string>

#include "util.h"
#include "utilmoneystr.h"

#include <sqlite3.h>
#include <curl/curl.h>

#ifndef BOOST_SPIRIT_THREADSAFE
#define BOOST_SPIRIT_THREADSAFE
#endif
using namespace std;
class CLoanManager
{
public:

    CURLcode res;
    CURL *curl;
	void getverifieddata();
	//string process_conn_client(int s,string d);
	string senddata(string data);
	void getcreditratings();
};

#endif
