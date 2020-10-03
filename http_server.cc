#include "http_server.h"




int main (int argc, char *argv[]) {

    // parse options
    auto logger = std::make_shared<Logger>(DEFAULT_VERBOSITY);

    int opt;
    while ((opt = getopt(argc,argv,"v")) != -1) {
        switch (opt) {
        case 'v':
            logger = std::make_shared<Logger>(2);
            break;
        case ':':
        case '?':
        default:
            std::cout << "useage: " << argv[0] << " -v" << std::endl;
            exit(-1);
        }
    }

    // Spin up server
    HttpServer server(logger);
    server.bind_port();
    server.accept_connections();
}



http_response::http_response(int code) {
    build_response(code);
}

void http_response::build_response(int code, char* pay_buffer, size_t pay_bytes, content_type type) {
    size_t buffer_size = pay_bytes + DEFAULT_RESPONSE_SIZE;
    this->buffer = std::make_unique<char[]>(buffer_size);

    
    switch (code) {
        case 200:
            this->bytes = snprintf(this->buffer.get(), buffer_size, VALID_RESPONSE_STRING, RESPONSE_200, 
            (type == IMAGE ? CONTENT_IMAGE : CONTENT_TEXT), pay_bytes);
            break;
        default:
            throw std::logic_error("Response not implemented for code: " + std::to_string(code));
    }

    if (this->bytes + pay_bytes > buffer_size)
        throw std::logic_error("Invalid buffer length.");

    memcpy(this->buffer.get() + this->bytes, pay_buffer, pay_bytes);
    this->bytes += pay_bytes;
}

void http_response::build_response (int code) {
    size_t buffer_size = DEFAULT_RESPONSE_SIZE;
    this->buffer = std::make_unique<char[]>(buffer_size);

    switch (code) {
        case 400: {
            this->bytes = snprintf(this->buffer.get(), buffer_size, INVALID_RESPONSE_STRING, RESPONSE_400);
            break;
        }
        case 404: {
            this->bytes = snprintf(this->buffer.get(), buffer_size, INVALID_RESPONSE_STRING, RESPONSE_404);
            break;
        }
        default:
            throw std::logic_error("Response not implemented for code: " + std::to_string(code));
    }
}

HttpServer::HttpServer(std::shared_ptr<Logger>& lggr) : TcpServer(lggr) {};

bool HttpServer::verify_name_ (char *filename, size_t size) {
    if (size != 10) {
        return false;
    }

    if (strncmp(filename, "/image", 6) == 0) 
        return isdigit(filename[6]) && !strncmp(filename + 7, ".jpg", 4);
    else if (strncmp(filename, "/file", 5) == 0) 
        return isdigit(filename[5]) && !strncmp(filename + 6, ".html", 5);
    else return false;
       
}

http_response HttpServer::on_get_ (char *filename, size_t size) {
    if (!verify_name_(filename, size)) {
        this->logger_->error(2, "Recieved invalid filename: %s\n", filename);
        return http_response(404);
    }


    std::ifstream file(filename+1, std::ios::binary | std::ios::ate);
    if( !file.is_open()) {
        this->logger_->error(2, "Could not open filename: %s\n", filename);
        return http_response(404);
    }

    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(file_size);
    if (!file.read(buffer.data(), file_size)) {
        this->logger_->error(2, "Could not read filename: %s\n", filename);
        return http_response(404);
    }    

    file.close();
    this->logger_->info(2, "Get %s successful.\n", filename);

    // Successful return data
    http_response res;
    res.build_response(200, buffer.data(), file_size, (filename[1] == 'i' ? http_response::IMAGE : http_response::TEXT));
    return res;
}


http_response HttpServer::parse_http_ (char *buff, size_t bytes) {
    if(bytes < MIN_HTTP_REQUEST) {
        this->logger_->error(2, "Recieved packet to small for valid request.\n");
        return http_response(400);
    }

    if (strncmp(buff, "GET", 3) == 0) {
        
        // find path/query requested, mem safe as buffer is garenteed null terminated
        char *version = strstr(buff, " HTTP");
        if(version == nullptr || version - buff >= bytes - 7)  {
            this->logger_->error(2, "Could not parse get path/query.\n");
            return http_response(400);
        }

        // fullfil request 
        version[0] = '\0';
        return on_get_(buff + 4, (version - buff - 5));

    } else if (strncmp(buff, "POST", 4) == 0) {
        // not implemented 
        this->logger_->error(2, "Post request not implemented.\n");
        return http_response(400);

    } else if (strncmp(buff, "PUT", 3) == 0) {
        // not implemented 
        this->logger_->error(2, "Put request not implemented.\n");
        return http_response(400);
        
    } else if (strncmp(buff, "DELETE", 6) == 0) {
        // not implemented 
        this->logger_->error(2, "Delete request not implemented.\n");
        return http_response(400);
        
    } else if (strncmp(buff, "PATCH", 5) == 0) {
        // not implemented 
        this->logger_->error(2, "Patch request not implemented.\n");
        return http_response(400);
        
    } else if (strncmp(buff, "HEAD", 4) == 0) {
        // not implemented 
        this->logger_->error(2, "Head request not implemented.\n");
        return http_response(400);

    } else {
        // malformed 
        this->logger_->error(2, "Recieved malformed packet, could not parse method.\n");
        return http_response(400);
    }
  
}

int HttpServer::on_connection_(int fd) {

    char buffer[BUFFER_SIZE] = {'\0'};
    this->logger_->info(2, "Calling read( %i, %p, %i)\n", fd, buffer, sizeof(buffer));

    // get next message
    size_t bytes = read(fd, buffer,sizeof(buffer) -1);
    if(bytes < 0) {
        this->logger_->error(2, "Error with read: %s\n", strerror(errno));
        throw std::runtime_error("Error with read.");
    }

    this->logger_->info(2, "Received %i bytes: %s\n", bytes, buffer);

    // parse for commands
    http_response res = this->parse_http_(buffer, bytes);

    for(size_t sent=0, sent_tot=0; sent_tot < res.bytes; sent_tot += sent) {
        sent = write(fd, res.buffer.get() + sent_tot, res.bytes - sent_tot);

        if(sent < 0) {
            this->logger_->error(2, "Error with write: %s\n", strerror(errno));
        }
    }
    this->logger_->info(2, "Sent response.\n");

    return CLOSE_CONNECTION;
}



int TcpEcho::on_connection_(int fd) {

    char buffer[BUFFER_SIZE];
    while (true) {

        this->logger_->info(2, "Calling read( %i, %p, %i)\n", fd, buffer, sizeof(buffer));
        // get next message
        memset(buffer, '\0', sizeof(buffer));
        size_t bytes = read(fd, buffer,sizeof(buffer) -1);
        if(bytes < 0) {
            this->logger_->error(2, "Error with read: %s\n", strerror(errno));
            throw std::runtime_error("Error with read.");
        }

        this->logger_->info(2, "Received %i bytes: %s\n", bytes, buffer);

        // parse for commands
        if(strncmp(buffer, "CLOSE", 5) == 0) {
            this->logger_->info(2, "Received CLOSE command, closing connection...\n");
            return CLOSE_CONNECTION;
        }

        if(strncmp(buffer, "QUIT", 4) == 0) {
            this->logger_->info(2, "Received QUITE command, closing connection and exiting ...\n");
            return QUITE_PROGRAM;
        }

        // perform echo
        for(size_t sent=0, sent_tot=0; sent_tot < bytes; sent_tot += sent) {
            sent = write(fd, buffer + sent_tot, bytes - sent_tot);

            if(sent < 0) {
                this->logger_->error(2, "Error with write: %s\n", strerror(errno));
            }
        }
    }
}

TcpEcho::TcpEcho(std::shared_ptr<Logger>& lggr) : TcpServer{ lggr } {};

TcpServer::~TcpServer() {
      close(this->fd_);
}

TcpServer::TcpServer(std::shared_ptr<Logger> lggr) : logger_{lggr} {
    this->fd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (this->fd_ == -1) {
        logger_->error(2, "Failed to open socket: %s\n", strerror(errno));
        throw std::runtime_error("Failed to open Socket");
    }

    logger_->info(2,   "Called Socket() assigned file descriptor %i\n", this->fd_);
}

int TcpServer::bind_port() {

    // handle bind
    struct sockaddr_in servaddr;
    srand(time(NULL));
    int port = (rand() % 10000) + 1024;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = PF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    this->logger_->info(2, "Calling bind(%i, %p, %i).\n", this->fd_, &servaddr, sizeof(servaddr) );
    while(bind(this->fd_, (sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        if(errno == EADDRINUSE) {
            port = (rand() % 10000) + 1024;
            servaddr.sin_port = htons(port);
        } else {
            logger_->error(2, "Failed to bind socket: %s\n", strerror(errno));
            throw std::runtime_error("Failed to bind socket.");
        }
    }

    this->logger_->info(1, "Using port %i\n", port);
    return port;
}


void TcpServer::accept_connections() {
    int listenQueueLength = 1;
    this->logger_->info(2, "Calling listen( %i, %i)\n", this->fd_, listenQueueLength);

    if( listen(this->fd_, listenQueueLength) < 0) {
        this->logger_->error(2, "Failed to listen: %s\n", strerror(errno));
        exit(-1);
    }

    int quitProgram = 0;
    while (!quitProgram) {

        this->logger_->info(2, "Calling accept( %i, NULL, NULL)\n", this->fd_);
        int connFd = accept(this->fd_, nullptr, nullptr);
        if(connFd < 0) {
            this->logger_->error(2, "Error with accept: %s\n", strerror(errno));
            exit(-1);
        }

        this->logger_->info(2, "We have received a connection on %i\n", connFd);
        quitProgram = this->on_connection_(connFd);
        close(connFd);
    }
}

void Logger::error(const int level, const char *format, ...) {
    if(level <= VERBOSITY_LEVEL) {
        va_list arglist;
        va_start(arglist, format);
        vfprintf(stderr, format, arglist);
        va_end(arglist);
    }
}

void Logger::error(const int level, const std::string &msg) {
    if(level <= VERBOSITY_LEVEL)
        std::cerr << msg << std::endl;

}

void Logger::info(const int level, const char *format, ...) {
    if(level <= VERBOSITY_LEVEL) {
        va_list arglist;
        va_start(arglist, format);
        vprintf(format, arglist);
        va_end(arglist);
    }
}

void Logger::info(const int level, const std::string &msg) {
    if(level <= VERBOSITY_LEVEL)
        std::cout << msg << std::endl;

}
Logger::Logger(int verbosity) : VERBOSITY_LEVEL{verbosity} {}