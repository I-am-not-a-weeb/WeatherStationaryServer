#pragma warning(disable:4996)

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
utility::string_t esp_url = L"http://localhost:8080";

const auto mqtt_server_url = "mqtt://localhost:1883/";

web::http::client::http_client* http_client;

struct struct_date;
struct struct_growbox_data;

std::string curr_formated_data_string={};
std::fstream curr_date_file, auth_file;

std::vector <utility::string_t> auth_vec = {};

void handle_get(web::http::http_request request);
void handle_patch(web::http::http_request request);
void handle_delete(web::http::http_request request);
void handle_post(web::http::http_request request);

struct worker{ std::wstring name; std::wstring surname; };

std::vector< worker > workers_vec;
std::vector<std::wstring> items_vec, orders_vec;

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

                        std::string kek{ std::format("{}:{}:{} {} {} {} {} {}",     /// sub to change
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
    listener.support(web::http::methods::PATCH, std::bind(handle_patch, std::placeholders::_1));
    bool listenening = false;

    http_client = new web::http::client::http_client(esp_url);

    /*
    web::websockets::client::websocket_client ws_client;
    ws_client.connect(U("ws://localhost:8080")).then([&](pplx::task<void> task)
    {
	    	try
	    	{
	    		task.get();
				std::cout << "Connected to websocket." << std::endl;
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error: " << e.what() << std::endl;
			}
	});
	*/

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
                std::string com;

                std::cin >> com;
                if(com=="start")
                {
                    if(listenening)
                    {
                    	std::cout << "Server is already listening." << std::endl;
						continue;
					}
                	
                    listener.open().then([&]
                        {
                            std::wcout << L"Starting listening on address: " << server_url << std::endl;
							listenening = true;
                        });
                }
                else if(com=="ping")
                {
                    std::string command{ std::format("ping {}\n",utility::conversions::to_utf8string(esp_url)) };
                    system(command.c_str());
                }
                else if(com=="set")
                {
                    std::string what;

                    std::cin >> what;
                }
                else
                {
                	std::cout << "Unknown command." << std::endl;
					std::cout << "Available commands:" << std::endl;
                    std::cout << "start -  starts server, begins answering http requests" << std::endl;
                    std::cout << "ping  -  pings client esp" << std::endl;
                    std::cout << "set   -  sets esp settings" << std::endl;
                }
	        }
		});
    task_client.wait();
	return 0;
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
            request.reply(web::http::status_codes::BadRequest);
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

                    utility::string_t time_greaterthan{ L"00:00:00" }, time_lessthan{ L"24:00:00" };
                    double temp_greaterthan{ 0 }, temp_lessthan{ 101 };
                    double humi_greaterthan{ 0 }, humi_lessthan{ 101 };
                    double ppm_greaterthan{ 0 }, ppm_lessthan{ 100001 };
                    double lux_greaterthan{ 0 }, lux_lessthan{ 10001 };
                    double rpm_greaterthan{0 }, rpm_lessthan{ 10001 };

                    if (query.contains(L"filter"))
                    {
                        web::json::object json_query_filter = web::json::value::parse(query[L"filter"]).as_object();

                        if (json_query_filter[L"time"].is_object())
                        {
                            web::json::object json_query_filter_time = json_query_filter[L"time"].as_object();

                            if (json_query_filter_time[L"gt"].is_string())
                            {
                                time_greaterthan = json_query_filter_time[L"gt"].as_string();
                            }

                            if (json_query_filter_time[L"lt"].is_string())
                            {
                                time_lessthan = json_query_filter_time[L"lt"].as_string();
                            }
                        }

                        if (json_query_filter[L"temp"].is_object())
                        {
                            web::json::object json_query_filter_temp = json_query_filter[L"temp"].as_object();

                            if (json_query_filter_temp[L"gt"].is_string())
                            {
                                temp_greaterthan = stod(json_query_filter_temp[L"gt"].as_string());
                            }

                            if (json_query_filter_temp[L"lt"].is_string())
                            {
                                temp_lessthan = stod(json_query_filter_temp[L"lt"].as_string());
                            }
                        }

                        if (json_query_filter[L"humi"].is_object())
                        {
                            web::json::object json_query_filter_humi = json_query_filter[L"humi"].as_object();

                            if (json_query_filter_humi[L"gt"].is_string())
                            {
                                humi_greaterthan = stod(json_query_filter_humi[L"gt"].as_string());
                            }

                            if (json_query_filter_humi[L"lt"].is_string())
                            {
                                humi_lessthan = stod(json_query_filter_humi[L"lt"].as_string());
                            }
                        }

                        if (json_query_filter[L"ppm"].is_object())
                        {
                            web::json::object json_query_filter_ppm = json_query_filter[L"ppm"].as_object();

                            if (json_query_filter_ppm[L"gt"].is_string())
                            {
                                ppm_greaterthan = stod(json_query_filter_ppm[L"gt"].as_string());
                            }
                            if (json_query_filter_ppm[L"lt"].is_string())
                            {
                                ppm_lessthan = stod(json_query_filter_ppm[L"lt"].as_string());
                            }
                        }

                        if (json_query_filter[L"lux"].is_object())
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

                        if (json_query_filter[L"rpm"].is_object())
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
                    return;
                }
                else
                {
                    request.reply(web::http::status_codes::NotFound);
                    return;
                }
            }
        }
        else if(path[0] == L"workers")
        {
	        if(path.size()==1)
	        {
	        	web::json::value json_response;
				web::json::value json_response_workers = web::json::value::array();

				for(auto i : workers_vec)
				{
					web::json::value json_response_worker;
					json_response_worker[L"name"] = web::json::value::string(i.name);
					json_response_worker[L"surname"] = web::json::value::string(i.surname);

					json_response_workers[json_response_workers.size()] = json_response_worker;
				}
				json_response[L"workers"] = json_response_workers;

				request.reply(web::http::status_codes::OK, json_response);
				return;
			}
			else if(path.size()==2)
			{
				web::json::value json_response;
				web::json::value json_response_worker;

				for(auto i : workers_vec)
				{
					if(i.surname == path[1])
					{
						json_response_worker[L"name"] = web::json::value::string(i.name);
						json_response_worker[L"surname"] = web::json::value::string(i.surname);

						json_response[L"worker"] = json_response_worker;

						request.reply(web::http::status_codes::OK, json_response);
						return;
					}
				}
				request.reply(web::http::status_codes::NotFound);
				return;
			}
		}
		else if(path[0] == L"items")
		{
			if(path.size()==1)
			{
				web::json::value json_response;
				web::json::value json_response_items = web::json::value::array();

				for(auto i : items_vec)
				{
					json_response_items[json_response_items.size()] = web::json::value::string(i);
				}
				json_response[L"items"] = json_response_items;

				request.reply(web::http::status_codes::OK, json_response);
				return;
			}
			else if(path.size()==2)
			{
				web::json::value json_response;

				for(auto i : items_vec)
				{
					if(i == path[1])
					{
						json_response[L"item"] = web::json::value::string(i);

						request.reply(web::http::status_codes::OK, json_response);
						return;
					}
				}
				request.reply(web::http::status_codes::NotFound);
				return;
			}
		}
        else if(path[0]==L"orders")
        {
	        if(path.size()==1)
	        {
	        	web::json::value json_response;
				web::json::value json_response_orders = web::json::value::array();

				for(auto i : orders_vec)
				{
					json_response_orders[json_response_orders.size()] = web::json::value::string(i);
				}
				json_response[L"orders"] = json_response_orders;

				request.reply(web::http::status_codes::OK, json_response);
				return;
			}
			else if(path.size()==2)
			{
				web::json::value json_response;

				for(auto i : orders_vec)
				{
					if(i == path[1])
					{
						json_response[L"order"] = web::json::value::string(i);

						request.reply(web::http::status_codes::OK, json_response);
						return;
					}
				}
				request.reply(web::http::status_codes::NotFound);
				return;
			}
        }
        request.reply(web::http::status_codes::BadRequest);
    }
    catch (const std::exception& e)
    {
    	std::cerr << "Error: " << e.what() << std::endl;
		request.reply(web::http::status_codes::NotAcceptable,e.what());
	}   
}

void handle_patch(web::http::http_request request)
{
	try
	{
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

        const std::vector path{ web::http::uri::split_path(web::http::uri::decode(request.relative_uri().path())) };
		if (path.empty())
		{
			request.reply(web::http::status_codes::BadRequest);
			return;
		}
		else if (path[0] == L"settings")                           //setttings change endpoint
		{
            if(path.size()==1)                          
            {
                web::json::value json{ request.extract_json().get() };
            	http_client->request(web::http::methods::PUT, L"/settings",json).then([&](web::http::http_response inner_response)
            		{
                        request.reply(inner_response.status_code());
                        return;
					});
            }
		}
        else if(path[0]==L"workers")
        {
	       web::json::value json_request{ request.extract_json().get() };

			if (!json_request.has_field(L"name") || !json_request.has_field(L"surname") || !json_request.has_field(L"new_surname"))
			{
				request.reply(web::http::status_codes::BadRequest);
				return;
			}

			for (auto i = workers_vec.begin(); i != workers_vec.end(); i++)
			{
				if (i->surname == json_request.at(L"surname").as_string())
				{
                    i->name = json_request.at(L"new_name").as_string();
					i->surname = json_request.at(L"new_surname").as_string();

					request.reply(web::http::status_codes::OK);
					return;
				}
			}
			request.reply(web::http::status_codes::NotFound);
			return;
		}
		else if(path[0] == L"items")
		{
	        web::json::value json_request{ request.extract_json().get() };

            if(!json_request.has_field(L"name"))
            {
	            request.reply(web::http::status_codes::BadRequest);
            }

            for(auto i = items_vec.begin(); i!=items_vec.end(); i++)
            {
	            if(*i == json_request.at(L"name").as_string())
	            {
	            	*i = json_request.at(L"new_name").as_string();

					request.reply(web::http::status_codes::OK);
					return;
				}
            }
            request.reply(web::http::status_codes::NotFound);
            return;
        }
        else if(path[0] == L"orders")
        {
        	web::json::value json_request{ request.extract_json().get() };

            for(auto i = orders_vec.begin(); i!=orders_vec.end(); i++)
            {
                if(*i == json_request.at(L"order").as_string())
                {
	                *i = json_request.at(L"new_order").as_string();

                    request.reply(web::http::status_codes::OK);
                    return;
                }
            }
            request.reply(web::http::status_codes::NotFound);
            return;
		}

        request.reply(web::http::status_codes::BadRequest);
	}
	catch(std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		request.reply(web::http::status_codes::NotAcceptable, e.what());
	}
}

void handle_delete(web::http::http_request request)
{
	try
	{
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

		const std::vector path{ web::http::uri::split_path(web::http::uri::decode(request.relative_uri().path())) };
		if (path.empty())
		{
			request.reply(web::http::status_codes::BadRequest);
			return;
		}
		else if (path[0] == L"data")                           
		{
            if (path.size() == 2 && path[1].size() == 10)      //data/:DATE endpoint
            {
                web::json::value json_request{ request.extract_json().get() };

                std::string date = utility::conversions::to_utf8string(path[1]);
                std::ifstream file;
                file.open(date + ".txt", std::ios::in);
                if (file.is_open())
                {
                    std::stringstream tmp_ss;

                    utility::string_t time_greaterthan{ L"00:00:00" }, time_lessthan{ L"24:00:00" };
                    double temp_greaterthan{ 0 }, temp_lessthan{ 101 };
                    double humi_greaterthan{ 0 }, humi_lessthan{ 101 };
                    double ppm_greaterthan{ 0 }, ppm_lessthan{ 100001 };
                    double lux_greaterthan{ 0 }, lux_lessthan{ 10001 };
                    double rpm_greaterthan{ 0 }, rpm_lessthan{ 10001 };


                    if (json_request[L"time"].is_object())
                    {
                        web::json::object json_request_time = json_request[L"time"].as_object();

                        if (json_request_time[L"gt"].is_string())
                        {
                            time_greaterthan = json_request_time[L"gt"].as_string();
                        }

                        if (json_request_time[L"lt"].is_string())
                        {
                            time_lessthan = json_request_time[L"lt"].as_string();
                        }
                    }

                    if (json_request[L"temp"].is_object())
                    {
                        web::json::object json_request_temp = json_request[L"temp"].as_object();

                        if (json_request_temp[L"gt"].is_string())
                        {
                            temp_greaterthan = stod(json_request_temp[L"gt"].as_string());
                        }

                        if (json_request_temp[L"lt"].is_string())
                        {
                            temp_lessthan = stod(json_request_temp[L"lt"].as_string());
                        }
                    }

                    if (json_request[L"humi"].is_object())
                    {
                        web::json::object json_request_humi = json_request[L"humi"].as_object();

                        if (json_request_humi[L"gt"].is_string())
                        {
                            humi_greaterthan = stod(json_request_humi[L"gt"].as_string());
                        }

                        if (json_request_humi[L"lt"].is_string())
                        {
                            humi_lessthan = stod(json_request_humi[L"lt"].as_string());
                        }
                    }

                    if (json_request[L"ppm"].is_object())
                    {
                        web::json::object json_request_ppm = json_request[L"ppm"].as_object();

                        if (json_request_ppm[L"gt"].is_string())
                        {
                            ppm_greaterthan = stod(json_request_ppm[L"gt"].as_string());
                        }
                        if (json_request_ppm[L"lt"].is_string())
                        {
                            ppm_lessthan = stod(json_request_ppm[L"lt"].as_string());
                        }
                    }

                    if (json_request[L"lux"].is_object())
                    {
                        web::json::object json_request_lux = json_request[L"lux"].as_object();

                        if (json_request_lux[L"gt"].is_string())
                        {
                            lux_greaterthan = stod(json_request_lux[L"gt"].as_string());
                        }
                        if (json_request_lux[L"lt"].is_string())
                        {
                            lux_lessthan = stod(json_request_lux[L"lt"].as_string());
                        }
                    }

                    if (json_request[L"rpm"].is_object())
                    {
                        web::json::object json_request_rpm = json_request[L"rpm"].as_object();

                        if (json_request_rpm[L"gt"].is_string())
                        {
                            rpm_greaterthan = stod(json_request_rpm[L"gt"].as_string());
                        }
                        if (json_request_rpm[L"lt"].is_string())
                        {
                            rpm_lessthan = stod(json_request_rpm[L"lt"].as_string());
                        }
                    }

                    while (!file.eof())                     /// reading file line by line and comparing to filter query
                    {
                        std::string time;
                        double temp, humi, ppm, lux, rpm;

                        file >> time >> temp >> humi >> ppm >> lux >> rpm;

                        utility::string_t time_t = utility::conversions::to_string_t(time);

                        if (time_t <= time_greaterthan || time_t >= time_lessthan &&
                            temp <= temp_greaterthan || temp >= temp_lessthan &&
                            humi <= humi_greaterthan || humi >= humi_lessthan &&
                            ppm <= ppm_greaterthan || ppm >= ppm_lessthan &&
                            lux <= lux_greaterthan || lux >= lux_lessthan &&
                            rpm <= rpm_greaterthan || rpm >= rpm_lessthan)
                        {
                            tmp_ss << std::format("{} {} {} {} {} {}\n", time, temp, humi, ppm, lux, rpm);
                        }
                    }
                    file.clear();
                    file.close();
                    std::ofstream file_new;
                    file_new.open(date + ".txt", std::ios::out | std::ios::trunc);
                    file_new << tmp_ss.str();

                    file_new.close();
                    request.reply(web::http::status_codes::OK);
                    return;
                }
                else
                {
                    request.reply(web::http::status_codes::NotFound);
                    return;
                }
            }
		}
        else if(path[0] == L"workers")
        {
            web::json::value json_request{ request.extract_json().get() };

            if (!json_request.has_field(L"surname"))
            {
                request.reply(web::http::status_codes::BadRequest);
                return;
            }

            for(auto i = workers_vec.begin(); i!=workers_vec.end(); i++)
            {
                if (i->surname == json_request.at(L"surname").as_string())
                {
                    workers_vec.erase(i);
                    request.reply(web::http::status_codes::OK);
                }
			}
            request.reply(web::http::status_codes::NotFound);
            return;
        }
        else if(path[0] == L"items")
        {
	        web::json::value json_request{ request.extract_json().get() };

			if (!json_request.has_field(L"item_name"))
			{
				request.reply(web::http::status_codes::BadRequest);
				return;
			}

			for (auto i = items_vec.begin(); i != items_vec.end(); i++)
			{
				if (*i == json_request.at(L"item_name").as_string())
				{
					items_vec.erase(i);
                    request.reply(web::http::status_codes::OK);
				}
			}
            request.reply(web::http::status_codes::NotFound);
			return;
		}
		else if(path[0] == L"orders")
		{
			web::json::value json_request{ request.extract_json().get() };

			if (!json_request.has_field(L"order"))
			{
				request.reply(web::http::status_codes::BadRequest);
				return;
			}

			for (auto i = orders_vec.begin(); i != orders_vec.end(); i++)
			{
				if (*i == json_request.at(L"order").as_string())
				{
					orders_vec.erase(i);
                    request.reply(web::http::status_codes::OK);
                    return;
				}
			}
			request.reply(web::http::status_codes::NotFound);
			return;
		}
		request.reply(web::http::status_codes::BadRequest);
	}
	catch(std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		request.reply(web::http::status_codes::NotAcceptable, e.what());
	}
}

void handle_post(web::http::http_request request)
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

        web::json::value json{ request.extract_json().get() };

        const std::vector path{ web::http::uri::split_path(web::http::uri::decode(request.relative_uri().path())) };
        if (path.empty())
        {
            request.reply(web::http::status_codes::BadRequest);
            return;
        }
        else if (path[0] == L"workers")
        {
            if (!json.has_field(L"name") || !json.has_field(L"surname"))
            {
                request.reply(web::http::status_codes::BadRequest);
                return;
            }

            workers_vec.push_back({ json.at(L"name").as_string(),json.at(L"surname").as_string() });
            request.reply(web::http::status_codes::Created);
        }
        else if (path[0] == L"items")
        {
            if (!json.has_field(L"item_name"))
            {
                request.reply(web::http::status_codes::BadRequest);
                return;
            }

            items_vec.push_back(json.at(L"item_name").as_string());
            request.reply(web::http::status_codes::Created);
        }
        else if (path[0] == L"orders")
        {
            if (!json.has_field(L"order"))
            {
                request.reply(web::http::status_codes::BadRequest);
                return;
            }

            orders_vec.push_back(json.at(L"order").as_string());
            request.reply(web::http::status_codes::Created);
        }
        else
        {
            request.reply(web::http::status_codes::BadRequest);
            return;
        }
    }
	catch(std::exception& e)
	{
		std::cout << "Error: " << e.what() << std::endl;
	}
}