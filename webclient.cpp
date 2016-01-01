
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ssleay32.lib")
typedef SOCKET socket_t;
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#define closesocket(S) close(S)
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

#include "webclient.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/types.h>

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define USER_AGENT "Generic Web Client"

URI::URI(char const *str)
{
	port_ = 0;
	char const *left;
	char const *right;
	left = str;
	right = strstr(left, "://");
	if (right) {
		scheme_.assign(str, right - str);
		left = right + 3;
	}
	right = strchr(left, '/');
	if (right) {
		char const *p = strchr(left, ':');
		if (p && left < p && p < right) {
			int n = 0;
			char const *q = p + 1;
			while (q < right) {
				if (isdigit(*q & 0xff)) {
					n = n * 10 + (*q - '0');
				} else {
					n = -1;
					break;
				}
				q++;
			}
			host_.assign(left, p - left);
			if (n > 0 && n < 65536) {
				port_ = n;
			}
		} else {
			host_.assign(left, right - left);
		}
		path_ = right;
	}
}

bool URI::isssl() const
{
	if (scheme() == "https") return true;
	if (scheme() == "http") return false;
	if (port() == 443) return true;
	return false;
}

void WebClient::initialize()
{
#ifdef WIN32
	WSADATA wsaData;
	WORD wVersionRequested;
	wVersionRequested = MAKEWORD(1,1);
	WSAStartup(wVersionRequested, &wsaData);
	atexit((void (*)(void))(WSACleanup));
#endif
}

WebClient::Error WebClient::error() const
{
	return data.error;
}

void WebClient::clear_error()
{
	data.error = Error();
}

static std::string get_ssl_error()
{
	char tmp[1000];
	unsigned long e = ERR_get_error();
	ERR_error_string_n(e, tmp, sizeof(tmp));
	return tmp;
}

int WebClient::get_port(URI const *uri, char const *scheme, char const *protocol)
{
	int port = uri->port();
	if (port < 1 || port > 65535) {
		struct servent *s;
		s = getservbyname(uri->scheme().c_str(), protocol);
		if (s) {
			port = ntohs(s->s_port);
		} else {
			s = getservbyname(scheme, protocol);
			if (s) {
				port = ntohs(s->s_port);
			}
		}
		if (port < 1 || port > 65535) {
			port = 80;
		}
	}
	return port;
}

static inline std::string to_s(size_t n)
{
	char tmp[100];
	sprintf(tmp, "%zu", n);
	return tmp;
}

void WebClient::set_default_headers(URI const &uri, Post const *post)
{
	add_header("Host: " + uri.host());
	add_header("User-Agent: " USER_AGENT);
	add_header("Accept: */*");
	add_header("Connection: close");
	if (post) {
		add_header("Content-Length: " + to_s(post->data.size()));	
		add_header("Content-Type: application/x-www-form-urlencoded");
	}
}

std::string WebClient::make_http_request(URI const &uri, Post const *post)
{
	std::string str;

	str = post ? "POST " : "GET ";
	str += uri.path();
	str += " HTTP/1.0";
	str += "\r\n";

	for (std::vector<std::string>::const_iterator it = data.headers.begin(); it != data.headers.end(); it++) {
		str += *it;
		str += "\r\n";
	}

	str += "\r\n";
	return str;
}

void WebClient::parse_http_result(unsigned char const *begin, unsigned char const *end, std::vector<std::string> *headers, std::vector<unsigned char> *content)
{
	if (begin < end) {
		unsigned char const *left = begin;
		unsigned char const *right = left;
		while (1) {
			if (right >= end) {
				break;
			}
			if (*right == '\r' || *right == '\n') {
				if (left < right) {
					headers->push_back(std::string(left, right));
				}
				if (right + 1 < end && *right == '\r' && right[1] == '\n') {
					right++;
				}
				right++;
				if (*right == '\r' || *right == '\n') {
					if (right + 1 < end && *right == '\r' && right[1] == '\n') {
						right++;
					}
					right++;
					left = right;
					break;
				}
				left = right;
			} else {
				right++;
			}
		}
		if (left < end) {
			content->assign(left, end);
		}
	}
}

void WebClient::parse_http_result(unsigned char const *begin, unsigned char const *end, Result *out)
{
	*out = Result();
	parse_http_result(begin, end, &out->headers, &out->content);
}

static void send_(socket_t s, char const *ptr, int len)
{
	while (len > 0) {
		int n = send(s, ptr, len, 0);
		if (n < 1 || n > len) {
			throw WebClient::Error("send request failed.");
		}
		ptr += n;
		len -= n;
	}
}

bool WebClient::http_get(URI const &uri, Post const *post, std::vector<char> *out)
{
	clear_error();
	out->clear();
	try {
		socket_t s;
		struct hostent *servhost; 
		struct sockaddr_in server;

		servhost = gethostbyname(uri.host().c_str());
		if (!servhost) {
			throw Error("gethostbyname failed.");
		}

		memset((char *)&server, 0, sizeof(server));
		server.sin_family = AF_INET;

		memcpy((char *)&server.sin_addr, servhost->h_addr, servhost->h_length);

		server.sin_port = htons(get_port(&uri, "http", "tcp"));

		s = socket(AF_INET, SOCK_STREAM, 0); 
		if (s == INVALID_SOCKET) {
			throw Error("socket failed.");
		}

		if (connect(s, (struct sockaddr*) &server, sizeof(server)) == SOCKET_ERROR) {
			throw Error("connect failed.");
		}

		set_default_headers(uri, post);

		std::string request = make_http_request(uri, post);

		send_(s, request.c_str(), (int)request.size());
		if (post && !post->data.empty()) {
			send_(s, (char const *)&post->data[0], (int)post->data.size());
		}

		while (1) {
			char buf[4096];
			int n = recv(s, buf, sizeof(buf), 0);
			if (n < 1) break;
			out->insert(out->end(), buf, buf + n);
		}

		closesocket(s);

		return true;
	} catch (Error const &e) {
		data.error = e;
	}
	return false;
}

static void ssend_(SSL *ssl, char const *ptr, int len)
{
	while (len > 0) {
		int n = SSL_write(ssl, ptr, len);
		if (n < 1 || n > len) {
			throw WebClient::Error(get_ssl_error());
		}
		ptr += n;
		len -= n;
	}
}

bool WebClient::https_get(URI const &uri, Post const *post, std::vector<char> *out)
{
	clear_error();
	out->clear();
	try {
		int ret;
		socket_t s;
		struct hostent *servhost; 
		struct sockaddr_in server;

		SSL *ssl;
		SSL_CTX *ctx;

		servhost = gethostbyname(uri.host().c_str());
		if (!servhost) {
			throw Error("gethostbyname failed.");
		}

		memset((char *)&server, 0, sizeof(server));
		server.sin_family = AF_INET;

		memcpy((char *)&server.sin_addr, servhost->h_addr, servhost->h_length);

		server.sin_port = htons(get_port(&uri, "https", "tcp"));

		s = socket(AF_INET, SOCK_STREAM, 0); 
		if (s == INVALID_SOCKET) {
			throw Error("socket failed.");
		}

		if (connect(s, (struct sockaddr*) &server, sizeof(server)) == SOCKET_ERROR) {
			throw Error("connect failed.");
		}

		SSL_load_error_strings();
		SSL_library_init();
		ctx = SSL_CTX_new(SSLv23_client_method());
		if (!ctx) {
			throw Error(get_ssl_error());
		}

		ssl = SSL_new(ctx);
		if (!ssl) {
			throw Error(get_ssl_error());
		}

		ret = SSL_set_fd(ssl, s);
		if (ret == 0) {
			throw Error(get_ssl_error());
		}

		RAND_poll();
		while (RAND_status() == 0) {
			unsigned short rand_ret = rand() % 65536;
			RAND_seed(&rand_ret, sizeof(rand_ret));
		}

		ret = SSL_connect(ssl);
		if (ret != 1) {
			throw Error(get_ssl_error());
		}

		set_default_headers(uri, post);

		std::string request = make_http_request(uri, post);

		ssend_(ssl, request.c_str(), (int)request.size());
		if (post && !post->data.empty()) {
			ssend_(ssl, (char const *)&post->data[0], (int)post->data.size());
		}

		while (1) {
			char buf[4096];
			int n = SSL_read(ssl, buf, sizeof(buf));
			if (n < 1) break;
			out->insert(out->end(), buf, buf + n);
		}

		SSL_shutdown(ssl); 
		closesocket(s);

		SSL_free(ssl); 
		SSL_CTX_free(ctx);
		ERR_free_strings();

		return true;
	} catch (Error const &e) {
		data.error = e;
	}
	return false;
}

void WebClient::get(URI const &uri, Post const *post, Result *out)
{
	*out = Result();
	std::vector<char> res;
	if (uri.isssl()) {
		https_get(uri, post, &res);
	} else {
		http_get(uri, post, &res);
	}
	if (!res.empty()) {
		unsigned char const *begin = (unsigned char const *)&res[0];
		unsigned char const *end = begin + res.size();
		parse_http_result(begin, end, out);
	}
}

void WebClient::get(URI const &uri)
{
	get(uri, 0, &data.result);
}

void WebClient::post(URI const &uri, Post const *post)
{
	get(uri, post, &data.result);
}

void WebClient::add_header(std::string const &text)
{
	data.headers.push_back(text);
}

WebClient::Result const &WebClient::result() const
{
	return data.result;
}
