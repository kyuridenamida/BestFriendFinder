
#ifndef __WebClient_h
#define __WebClient_h

#include <vector>
#include <string>

class URI {
private:
	std::string scheme_;
	std::string host_;
	int port_;
	std::string path_;
public:
	URI(char const *str);
	std::string const &scheme() const { return scheme_; }
	std::string const &host() const { return host_; }
	int port() const { return port_; }
	std::string const &path() const { return path_; }
	bool isssl() const;
};



class WebClient {
public:
	struct Error {
		std::string message;
		Error()
		{
		}
		Error(std::string const &message)
			: message(message)
		{
		}
	};
	struct Result {
		std::vector<std::string> headers;
		std::vector<unsigned char> content;
	};
	struct Post {
		std::vector<unsigned char> data;
	};
private:
	struct {
		std::vector<std::string> headers;
		Error error;
		Result result;
	} data;
	void clear_error();
	static int get_port(URI const *uri, char const *scheme, char const *protocol);
	void set_default_headers(URI const &uri, Post const *post);
	std::string make_http_request(URI const &uri, Post const *post);
	void parse_http_result(unsigned char const *begin, unsigned char const *end, std::vector<std::string> *headers, std::vector<unsigned char> *content);
	void parse_http_result(unsigned char const *begin, unsigned char const *end, Result *out);
	bool http_get(URI const &uri, Post const *post, std::vector<char> *out);
	bool https_get(URI const &uri, Post const *post, std::vector<char> *out);
	void get(URI const &uri, Post const *post, Result *out);
public:
	static void initialize();
	Error error() const;
	void get(URI const &uri);
	void post(URI const &uri, Post const *post);
	void add_header(std::string const &text);
	Result const &result() const;
};


#endif
