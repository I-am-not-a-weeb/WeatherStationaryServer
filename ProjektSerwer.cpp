﻿#pragma warning(disable:4996)

#include <iostream>
#include <fstream>
#include <ctime>
#include <format>


#include <pplx/pplxtasks.h>
#include <cpprest/details/http_server.h>
#include <cpprest/http_listener.h>
#include <cpprest/http_client.h>
#include <cpprest/ws_client.h>
#include <mqtt/async_client.h>

#include <cpprest/json.h>

const auto server_url = L"http://localhost:7475/";
const auto client_url = L"http://localhost:8080";

const auto mqtt_server_url = "mqtt://localhost:1883/";

struct struct_date;
struct struct_growbox_data;

std::string curr_formated_data_string={};
std::fstream curr_date_file, auth_file;

std::vector <utility::string_t> auth_vec = {};

void handle_get(web::http::http_request request);

struct struct_date
{
    int hour : 5;
    int minute : 6;
    int second : 6;
};
struct struct_growbox_data
{
    struct_date date;
    int temp : 8;
    unsigned int humi : 7;
    unsigned int ppm : 16;
    unsigned int lux : 16;
    unsigned int rpm : 15;
};

class action_listener : public virtual mqtt::iaction_listener
{
    std::string name_;

    void on_failure(const mqtt::token& tok) override {
        std::cout << name_ << " failure";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        std::cout << std::endl;
    }

    void on_success(const mqtt::token& tok) override {
        std::cout << name_ << " success";
        if (tok.get_message_id() != 0)
            std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
        auto top = tok.get_topics();
        if (top && !top->empty())
            std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
        std::cout << std::endl;
    }

public:
    action_listener(const std::string& name) : name_(name) {}
};

class mqtt_callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
    int nretry_;
    // The MQTT client
    mqtt::async_client& cli_;
    // Options to use if we need to reconnect
    mqtt::connect_options& connOpts_;

    action_listener subListener_;
public:
    mqtt_callback(mqtt::async_client& cli, mqtt::connect_options& connOpts)
        : nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
private:
    void reconnect() {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        try {
            cli_.connect(connOpts_, nullptr, subListener_);
        }
        catch (const mqtt::exception& exc) {
            std::cerr << "Error: " << exc.what() << std::endl;
            //exit(1);
        }
    }

    void on_failure(const mqtt::token& tok) override
	{
        std::cout << "Connection attempt failed" << std::endl;
        reconnect();
    }

    void on_success(const mqtt::token& tok)  override {}

    void connected(const std::string& cause) override {
        std::cout << "\nConnection success" << std::endl;

        cli_.subscribe("/update", 2, nullptr, subListener_);
    }

	void connection_lost(const std::string& cause) override
	{
		std::cout << "\nConnection lost" << std::endl;
		if (!cause.empty())	std::cout << "\tcause: " << cause << std::endl;
	}

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

	void message_arrived(mqtt::const_message_ptr msg) override
	{
        std::cout << "Got message: ";
        if(msg->get_topic() == "/update")
        {
            std::cout << "/update.";
            pplx::task<void> task = pplx::create_task([&,msg]()
                {
                    try 
                    {
                        std::string payload(msg->to_string());

                        std::cout << payload << std::endl;

                        web::json::value json = web::json::value::parse(payload.c_str());
                        const std::time_t now = time(0);

                        std::tm* ltm = localtime(&now);

                        std::string formated_date_string = std::to_string(ltm->tm_year + 1900) +
                            "-" + ((ltm->tm_mon > 10) ? std::to_string(ltm->tm_mon + 1) : ("0")+ std::to_string(ltm->tm_mon+1)) +
                            "-" + (ltm->tm_mday > 10 ? std::to_string(ltm->tm_mday) : ("0") + std::to_string(ltm->tm_mday));

                        if(curr_formated_data_string!=formated_date_string)
                        {
                        	curr_formated_data_string = formated_date_string;
						}

                        curr_date_file.open(formated_date_string + ".txt", std::ios::out | std::ios::app);

                        std::wcout << json.at(L"temp").as_string() << std::endl;
                        std::wcout << json.at(L"humi").as_string() << std::endl;
                        std::wcout << json.at(L"ppm").as_string() << std::endl;
                        std::wcout << json.at(L"lux").as_string() << std::endl;
                        std::wcout << json.at(L"rpm").as_string() << std::endl;

                        std::string kek{ std::format("{}:{}:{} {} {} {} {} {}",
                            ltm->tm_hour >= 10 ? std::to_string(ltm->tm_hour) : "0" + std::to_string(ltm->tm_hour),
                            ltm->tm_min >= 10 ? std::to_string(ltm->tm_min) : "0" + std::to_string(ltm->tm_min),
                            ltm->tm_sec >= 10 ? std::to_string(ltm->tm_sec) : "0" + std::to_string(ltm->tm_sec),
                            utility::conversions::to_utf8string(json.at(L"temp").as_string()),
                            utility::conversions::to_utf8string(json.at(L"humi").as_string()),
                            utility::conversions::to_utf8string(json.at(L"ppm").as_string()),
                            utility::conversions::to_utf8string(json.at(L"lux").as_string()),
                            utility::conversions::to_utf8string(json.at(L"rpm").as_string())
                        ) };

                        std::string append_to_file = {
                        	std::to_string(ltm->tm_hour) + ":" + (ltm->tm_min >= 10 ? std::to_string(ltm->tm_min) : "0" + std::to_string(ltm->tm_min)) + ":" + (ltm->tm_sec >= 10 ? std::to_string(ltm->tm_sec) : ("0") + std::to_string(ltm->tm_sec)) + " " +
							utility::conversions::to_utf8string(json.at(L"temp").as_string()) + " " +
                            utility::conversions::to_utf8string(json.at(L"humi").as_string()) + " " +
                            utility::conversions::to_utf8string(json.at(L"ppm").as_string()) + " " +
                            utility::conversions::to_utf8string(json.at(L"lux").as_string()) + " " +
                            utility::conversions::to_utf8string(json.at(L"rpm").as_string()) 
                        };

                        curr_date_file << append_to_file << std::endl;

                        curr_date_file.close();
                    }
					catch (const std::exception& e)
					{
                        
						std::cerr << "Error: " << e.what() << std::endl;
					}
                });
		}
        else
        {
        	std::cout << "unknown topic." << std::endl;
		}
	}
};

int main(int argc, char* argv[])
{
    auth_file.open("login.txt", std::ios::in);

    while(!auth_file.eof())
    {
        std::string login;
        auth_file >> login;
        auth_vec.push_back(utility::conversions::to_string_t(login));
	}

    web::http::experimental::listener::http_listener listener(server_url);
    listener.support(web::http::methods::GET, std::bind(handle_get,std::placeholders::_1));

    listener.open();

    web::http::client::http_client http_client(client_url);

    web::websockets::client::websocket_client ws_client();

    mqtt::async_client mqtt_client(mqtt_server_url,"GrowBoxServer");
    mqtt::connect_options mqtt_conn_opts;
    mqtt_conn_opts.set_keep_alive_interval(10000);
    mqtt_conn_opts.set_clean_session(true);

    mqtt_callback mqtt_cb(mqtt_client, mqtt_conn_opts);
    mqtt_client.set_callback(mqtt_cb);

    mqtt_client.connect(mqtt_conn_opts);

    std::string kek = "a";
    pplx::task<void> task_client = pplx::create_task([&]()          /// reading command line
	    {
	        while(true)
	        {
                std::string tmp;
                std::cin >> tmp;
                kek = tmp;
	        }
		});

    pplx::task<void> print = pplx::create_task([&]()
        {
            while (true)
            {
                Sleep(1000);
                //std::cout << std::endl << kek << std::endl;
            }
        });
        while (true);
}

void handle_get(web::http::http_request request)
{
    try {
        if (!request.headers().has(L"Authorization"))
        {
            request.reply(web::http::status_codes::Unauthorized, L"Unauthorized");
            return;
        }

        bool authenticated = false;
        for (auto i : auth_vec)
        {

            if (i == request.headers()[L"Authorization"].substr(6))
            {
                authenticated = true;
                break;
            }
        }

        if (!authenticated)
        {
            request.reply(web::http::status_codes::Unauthorized, L"Unauthorized");
			return;
        }

        std::vector<utility::string_t>path = web::http::uri::split_path(web::http::uri::decode(request.relative_uri().path()));

        std::map<utility::string_t, utility::string_t>query = web::http::uri::split_query(web::http::uri::decode(request.relative_uri().query()));

        if (path.empty())
        {
            request.reply(web::http::status_codes::ExpectationFailed);
            return;
        }
        else if (path[0] == L"data")                           //data endpoint
        {
            if (path.size() == 2 && path[1].size() == 10)      //data/:DATE endpoint
            {
                std::string date = utility::conversions::to_utf8string(path[1]);
                std::ifstream file;
                file.open(date + ".txt", std::ios::in);
                if (file.is_open())
                {
                    std::stringstream tmp_ss;


                    web::json::object json_query_filter = web::json::value::parse(query[L"filter"]).as_object();

                    utility::string_t time_greaterthan{ L"00:00:00" }, time_lessthan{ L"24:00:00" };
                    double temp_greaterthan{ 0 }, temp_lessthan{ 101 };
                    double humi_greaterthan{ 0 }, humi_lessthan{ 101 };
                    double ppm_greaterthan{ 0 }, ppm_lessthan{ 100001 };
                    double lux_greaterthan{ 0 }, lux_lessthan{ 10001 };
                    double rpm_greaterthan{0 }, rpm_lessthan{ 10001 };


                    if(json_query_filter[L"time"].is_object())
                    {
                    	web::json::object json_query_filter_time = json_query_filter[L"time"].as_object();

                        if(json_query_filter_time[L"gt"].is_string())
                        {
                            time_greaterthan = json_query_filter_time[L"gt"].as_string();
                        }

                    	if(json_query_filter_time[L"lt"].is_string())
                    	{
                            time_lessthan = json_query_filter_time[L"lt"].as_string();
                    	}
                    }

                    if(json_query_filter[L"temp"].is_object())
                    {
                    	web::json::object json_query_filter_temp = json_query_filter[L"temp"].as_object();

						if(json_query_filter_temp[L"gt"].is_string())
						{
							temp_greaterthan = stod(json_query_filter_temp[L"gt"].as_string());
						}

						if(json_query_filter_temp[L"lt"].is_string())
						{
							temp_lessthan = stod(json_query_filter_temp[L"lt"].as_string());
						}
					}

                    if(json_query_filter[L"humi"].is_object())
                    {
                    	web::json::object json_query_filter_humi = json_query_filter[L"humi"].as_object();

                        if(json_query_filter_humi[L"gt"].is_string())
						{
							humi_greaterthan = stod(json_query_filter_humi[L"gt"].as_string());
						}

						if(json_query_filter_humi[L"lt"].is_string())
						{
							humi_lessthan = stod(json_query_filter_humi[L"lt"].as_string());
						}
                    }

                    if(json_query_filter[L"ppm"].is_object())
                    {
                    	web::json::object json_query_filter_ppm = json_query_filter[L"ppm"].as_object();

						if(json_query_filter_ppm[L"gt"].is_string())
						{
							ppm_greaterthan = stod(json_query_filter_ppm[L"gt"].as_string());
						}
                        if(json_query_filter_ppm[L"lt"].is_string())
                        {
                            ppm_lessthan = stod(json_query_filter_ppm[L"lt"].as_string());
                        }
                    }

                    if(json_query_filter[L"lux"].is_object())
                    {
                    	web::json::object json_query_filter_lux = json_query_filter[L"lux"].as_object();

                        if (json_query_filter_lux[L"gt"].is_string())
                        {
                            lux_greaterthan = stod(json_query_filter_lux[L"gt"].as_string());
                        }
                        if (json_query_filter_lux[L"lt"].is_string())
                        {
                            lux_lessthan = stod(json_query_filter_lux[L"lt"].as_string());
                        }
                    }

                    if(json_query_filter[L"rpm"].is_object())
                    {
                    	web::json::object json_query_filter_rpm = json_query_filter[L"rpm"].as_object();

						if (json_query_filter_rpm[L"gt"].is_string())
						{
							rpm_greaterthan = stod(json_query_filter_rpm[L"gt"].as_string());
						}
						if (json_query_filter_rpm[L"lt"].is_string())
						{
							rpm_lessthan = stod(json_query_filter_rpm[L"lt"].as_string());
						}
					}

                    while (!file.eof())                     /// reading file line by line and comparing to filter query
                    {
                        std::string time;
                    	double temp, humi, ppm, lux, rpm;

                        file >> time >> temp >> humi >> ppm >> lux >> rpm;

                        utility::string_t time_t = utility::conversions::to_string_t(time);

                        if(time_t > time_greaterthan && time_t < time_lessthan &&
                        temp > temp_greaterthan && temp < temp_lessthan &&
                        humi > humi_greaterthan && humi < humi_lessthan &&
                        ppm > ppm_greaterthan && ppm < ppm_lessthan &&
                        lux > lux_greaterthan && lux < lux_lessthan &&
                        rpm > rpm_greaterthan && rpm < rpm_lessthan)
                        {
                            tmp_ss << std::format("{} {} {} {} {} {}\n", time, temp, humi, ppm, lux, rpm);
						}

                    }
                    file.close();
                    request.reply(web::http::status_codes::OK, tmp_ss.str());
                }
                else
                {
                    request.reply(web::http::status_codes::NotFound);
                }
            }
            else
            {
                request.reply(web::http::status_codes::ExpectationFailed);
            }
        }
    }
    catch (const std::exception& e)
    {
    	std::cerr << "Error: " << e.what() << std::endl;
		request.reply(web::http::status_codes::NotAcceptable,e.what());
	}   
}