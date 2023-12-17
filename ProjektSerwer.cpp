#pragma warning(disable:4996)

#include <iostream>
#include <fstream>
#include <ctime>

#include <pplx/pplxtasks.h>
#include <cpprest/details/http_server.h>
#include <cpprest/http_listener.h>
#include <cpprest/http_client.h>
#include <cpprest/ws_client.h>
#include <mqtt/async_client.h>

#include <cpprest/json.h>

const auto server_url = L"http://localhost:8080/";
const auto client_url = L"http://localhost:8080/";

const auto mqtt_server_url = "mqtt://localhost:1883/";

struct struct_date;
struct struct_growbox_data;

std::vector<struct_growbox_data> vec = {};

std::string curr_formated_data_string={};
std::fstream curr_date_file;

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

        cli_.subscribe("/update", 1, nullptr, subListener_);
    }

	void connection_lost(const std::string& cause) override
	{
		std::cout << "\nConnection lost" << std::endl;
		if (!cause.empty())	std::cout << "\tcause: " << cause << std::endl;
	}

    void delivery_complete(mqtt::delivery_token_ptr token) override {}

	void message_arrived(mqtt::const_message_ptr msg) override
	{
        if(msg->get_topic() == "/update")
        {
            pplx::task<void> task = pplx::create_task([&]()
                {
                    web::json::value json = web::json::value::parse(msg->get_payload_str());
                    try 
                    {
                        const std::time_t now = time(0);

                        std::tm* ltm = localtime(&now);

                        std::string formated_date_string = std::to_string(ltm->tm_year + 1900) +
                            "-" + ((ltm->tm_mon > 10) ? std::to_string(ltm->tm_mon + 1) : ("0")+ std::to_string(ltm->tm_mon+1)) +
                            "-" + (ltm->tm_mday > 10 ? std::to_string(ltm->tm_mday) : ("0") + std::to_string(ltm->tm_mday));

                        if(curr_formated_data_string!=formated_date_string)
                        {
                            curr_date_file.close();
                        	curr_formated_data_string = formated_date_string;
							curr_date_file.open(formated_date_string+".txt", std::ios::out | std::ios::app);
						}

                        json.at(L"temp").as_integer();
                        json.at(L"humi").as_integer();
                        json.at(L"ppm").as_integer();
                        json.at(L"lux").as_integer();
                        json.at(L"rpm").as_integer();

                        std::string append_to_file = {
                        	std::to_string(5+ltm->tm_hour) + ":" + std::to_string(30+ltm->tm_min) + ":" + std::to_string(ltm->tm_sec) + " " +
							std::to_string(json.at(L"temp").as_integer()) + " " +
							std::to_string(json.at(L"humi").as_integer()) + " " +
							std::to_string(json.at(L"ppm").as_integer()) + " " +
							std::to_string(json.at(L"lux").as_integer()) + " " +
							std::to_string(json.at(L"rpm").as_integer()) + "\n" 
                        };
                    }
					catch (const web::json::json_exception& exc)
					{
						std::cerr << "Error: " << exc.what() << std::endl;
					}
                });
		}   
	}
};

int main(int argc, char* argv[])
{
    web::http::experimental::listener::http_listener listener(server_url);


    web::http::client::http_client client(client_url);

    web::websockets::client::websocket_client ws_client();

    mqtt::async_client mqtt_client(mqtt_server_url,"GrowBoxServer");
    mqtt::connect_options mqtt_conn_opts;
    mqtt_conn_opts.set_keep_alive_interval(10);
    mqtt_conn_opts.set_clean_session(true);

    mqtt_callback mqtt_cb(mqtt_client, mqtt_conn_opts);
    mqtt_client.set_callback(mqtt_cb);

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
                std::cout << std::endl << kek << std::endl;
            }
        });
        while (true);
}
