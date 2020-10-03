#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <unistd.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdarg.h>

#include <memory>
#include <fstream>
#include <vector>

#define DEFAULT_VERBOSITY 1
#define CLOSE_CONNECTION 0
#define QUITE_PROGRAM 1

#define INVALID_RESPONSE_STRING "HTTP/1.1 %s\r\n\r\n"
#define VALID_RESPONSE_STRING "HTTP/1.1 %s\r\ncontent-type: %s\r\ncontent-length: %lu\r\n\r\n"

#define RESPONSE_400 "400  Bad Request"
#define RESPONSE_404 "404 File Not Found"
#define RESPONSE_200 "200 OK"

#define CONTENT_TEXT "text/html"
#define CONTENT_IMAGE "image/jpeg"

#define DEFAULT_RESPONSE_SIZE sizeof(VALID_RESPONSE_STRING) + sizeof(RESPONSE_200) + sizeof(CONTENT_IMAGE)
#define BUFFER_SIZE 32768
#define MIN_HTTP_REQUEST 16



/*
 * HELPER CLASSES
 */

class Logger {
public:
    Logger(int verbosity = 1);
    void info(const int level, const std::string &msg);
    void error(const int level, const std::string &msg);
    void info(const int level, const char *format , ... );
    void error(const int level, const char *format , ... );

private:
    const int VERBOSITY_LEVEL;

};

class http_response {
public:
    http_response() = default;
    explicit http_response(int code);

    size_t bytes = 0;
    std::unique_ptr<char[]> buffer;


    enum content_type {
        TEXT, BINARY, IMAGE
    };
    void build_response(int code, char *data, size_t bytes, content_type type);
    void build_response(int code);


private:


};

/*
 * SOCKET CLASSES
 */

class TcpServer {
public:
    TcpServer(std::shared_ptr<Logger> lggr);
    ~TcpServer();

    void accept_connections();
    int bind_port ();

protected:

    virtual int on_connection_ (int fd) = 0;

    std::shared_ptr<Logger> logger_;
    int fd_;
};

class TcpEcho : public TcpServer {
public:
    TcpEcho(std::shared_ptr<Logger>& lggr);

protected:

    virtual int on_connection_ (int fd) override;
};


class HttpServer : public TcpServer {
public:
    HttpServer(std::shared_ptr<Logger>& lggr);

private:

    virtual int on_connection_ (int fd) override;
    http_response parse_http_ (char* buff, size_t bytes);

    http_response on_get_ (char* filename, size_t size);
    bool verify_name_ (char* filename, size_t size);


};


#endif