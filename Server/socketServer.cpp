#include "socketServer.hpp"
#include "../response/GetResponse.hpp"

clients_info::clients_info() : file() {}

clients_info::clients_info(const clients_info &obj)
{
    *this = obj;
}

clients_info &clients_info::operator=(const clients_info &obj)
{
    addr_len = obj.addr_len;
    address = obj.address;
    socket_client_id = obj.socket_client_id;
    file.open(obj.path_file, std::ios::in | std::ios::binary | std::ios::out);
    file << obj.file.rdbuf();
    size = obj.size;
    header = obj.header;
    method = obj.method;
    path = obj.path;
    protocol = obj.protocol;
    host = obj.host;
    port = obj.port;
    map_request = obj.map_request;
    content_len = obj.content_len;
    content_len_exist = obj.content_len_exist;
    chunked_exist = obj.chunked_exist;
    len_rd = obj.len_rd;
    end_header = obj.end_header;
    end_chunk = obj.end_chunk;
    flag_header = obj.flag_header;
    flag_res = obj.flag_res;
    flag_req = obj.flag_req;
    flagRed = obj.flagRed;

    return *this;
}

clients_info::~clients_info() {}

SocketServer::SocketServer() : clients()
{
}

SocketServer::SocketServer(SocketServer const &cpy) : clients(cpy.clients)
{
}

SocketServer::~SocketServer()
{
}

int SocketServer::get_client()
{
    int index_client = 0;
    clients_info add_new;
    clients.push_back(add_new);
    index_client = clients.size() - 1;
    clients[index_client].addr_len = sizeof(clients[index_client].address);
    bzero(clients[index_client].request, 1024);
    bzero(clients[index_client].response, 1024);
    clients[index_client].content_len = 0;
    clients[index_client].content_len_exist = 0;
    clients[index_client].chunked_exist = 0;
    clients[index_client].len_rd = 0;
    clients[index_client].flag_header = 0;
    clients[index_client].flag_res = 0;
    clients[index_client].flag_req = 0;
    clients[index_client].flagRed = 0;
    clients[index_client].end_header = 0;
    clients[index_client].end_chunk = 0;
    return (index_client);
}

int  SocketServer::remove_client(int socket_client)
{
    size_t it_client = 0;
    while (it_client < clients.size())
    {
        if (socket_client == clients[it_client].socket_client_id)
        {
            std::cout << "Client Num : " << clients[it_client].socket_client_id << " is Diconnected\n";
            close(clients[it_client].socket_client_id);
            clients.erase(clients.begin() + (it_client));
            return it_client;
        }
        it_client++;
    }
    std::cout << it_client << "Not found\n";
    return 0;
}

void SocketServer::wait_clients(std::deque<server> &srv)
{
    FD_ZERO(&reads);
    FD_ZERO(&writer);
    size_t it_srv = 0;
    int max_socket = 0;
    while (it_srv < srv.size())
    {
        FD_SET(srv[it_srv].socket_id, &reads);
        max_socket = srv[it_srv].socket_id;
        it_srv++;
    }
    size_t it_client = 0;
    while (it_client < clients.size())
    {
        if (clients[it_client].socket_client_id > 0)
        {
            FD_SET(clients[it_client].socket_client_id, &reads);
            if (clients[it_client].socket_client_id > max_socket) {
                max_socket = clients[it_client].socket_client_id;
            }
        }
        it_client++;
    }
    it_client = 0;
    while (it_client < clients.size())
    {
        if (clients[it_client].socket_client_id > 0)
        {
            FD_SET(clients[it_client].socket_client_id, &writer);
            if (clients[it_client].socket_client_id > max_socket) {
                max_socket = clients[it_client].socket_client_id;
            }
        }
        it_client++;
    }
    if (select(max_socket + 1, &reads, &writer, 0, 0) < 0)
        std::cout << "Select : Failed\n";
}

void SocketServer::parse_request(int it_client)
{
    std::string key, value, walo, line;
    int pos;
    bzero(clients[it_client].request, 1024);
    if ((clients[it_client].len_rd <= clients[it_client].content_len) || (clients[it_client].end_chunk == 0 && clients[it_client].chunked_exist == 1)) {

        clients[it_client].len_rd += recv(clients[it_client].socket_client_id, clients[it_client].request, 1024, 0);
//        std::cout << "REQ : |" << clients[it_client].request << "|\n";
    }
    else
        clients[it_client].flag_res = 1;
    std::string req = clients[it_client].request;
    std::stringstream strm(req);
    if (clients[it_client].flag_req == 0)
    {
        size_t head = 0;
        getline(strm, clients[it_client].method, ' ');
        getline(strm, clients[it_client].path, ' ');
        getline(strm, clients[it_client].protocol, '\n');
        getline(strm, walo, ' ');
        head = clients[it_client].method.size() + 1 + clients[it_client].path.size() + 1 + clients[it_client].protocol.size() + walo.size() + 1;
        getline(strm, walo, '\n');
        pos = walo.find(":");
        if (pos == -1)
        {
            clients[it_client].host = walo;
            head += clients[it_client].host.size() + 1;
        }
        else
        {
            clients[it_client].host = walo.substr(0, pos);
            clients[it_client].port = walo.substr(pos+1, walo.length() - (pos + 2)) ;
            head += clients[it_client].host.size() + clients[it_client].port.size() + 2;
        }
        while (getline(strm, line, '\n'))
        {
            head += line.size();
            line = line.substr(0, line.size() - 1);
            pos = line.find(":");
            if (pos != -1)
            {
                key = line.substr(0, pos);
                value = line.substr(pos + 2);
                clients[it_client].map_request[key] = value;
                if (!key.compare("Content-Length"))
                    clients[it_client].content_len_exist = 1;
                else if (!value.compare("chunked"))
                    clients[it_client].chunked_exist = 1;
            }
            else
            {
                key = line;
                value = line;
            }
            if (key.size() == 0 && clients[it_client].end_header == 0)
            {
                clients[it_client].end_header = 1;
                if (clients[it_client].content_len_exist == 1)
                    clients[it_client].content_len = atol(clients[it_client].map_request["Content-Length"].c_str()) + head;
                if (clients[it_client].chunked_exist == 1)
                    clients[it_client].content_len = 0;
            }
            if (key == "0" && clients[it_client].chunked_exist == 1)
                clients[it_client].end_chunk = 1;
        }
        clients[it_client].flag_req = 1;
    }

    while (getline(strm, line, '\n'))
    {
        line = line.substr(0, line.size() - 1);
        pos = line.find(":");
        if (pos != -1)
        {
            key = line.substr(0, pos);
            value = line.substr(pos + 1);
        }
        else
        {
            key = line;
            value = line;
        }
        if (key == "0" && clients[it_client].chunked_exist == 1)
            clients[it_client].end_chunk = 1;
    }
}

void SocketServer::connection(std::deque<server> &srv)
{
    size_t it_srv;
    size_t it_client;
    while (1)
    {
        it_srv = 0;
        it_client = 0;
        wait_clients(srv);
        while (it_srv < srv.size())
        {
            if (FD_ISSET(srv[it_srv].socket_id, &reads))
            {
                it_client = get_client();
                clients[it_client].socket_client_id = accept(srv[it_srv].socket_id, (struct sockaddr *)&(clients[it_client].address), &clients[it_client].addr_len);
                if (clients[it_client].socket_client_id < 0)
                {
                    std::cerr << "Error in Connection\n";
                    return;
                }
                std::cout << "Client Num : " << clients[it_client].socket_client_id << " is Connected with Server : [" << srv[it_srv].host << ":" << srv[it_srv].port << "]\n";
                break;
            }
            it_srv++;
        }
        it_client = 0;
        while (it_client < clients.size())
        {
            if (FD_ISSET(clients[it_client].socket_client_id, &writer))
            {
                std::cout << "CLIENT : " << clients[it_client].socket_client_id << "\n";
                parse_request(it_client);

                if (clients[it_client].flag_res == 1)
                    GetResponse(srv, clients[it_client]);
                if ((clients[it_client].file.gcount() == 0 && clients[it_client].flag_res == 1 && clients[it_client].file.eof()) || clients[it_client].flagRed == true)
                {
                    clients[it_client].file.close();
                    it_client = remove_client(clients[it_client].socket_client_id);
                    clients[it_client].flagRed = false;
                    continue;
                }
            }
            it_client++;
        }
    }
}

void SocketServer::run_server(std::deque<server> &servers)
{
    size_t i = 0;
    // 1 - creat socket for each server
    while (i < servers.size())
    {
        servers[i].socket_id = socket(AF_INET, SOCK_STREAM, 0);
        if (servers[i].socket_id < 0)
            exit(1);
        std::cout << "Creating socket... " << servers[i].socket_id << std::endl;
        int opt_val = 1;
        setsockopt(servers[i].socket_id, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
        // 2 - binding socket ma3a l port d server
        struct sockaddr_in ServerAddr;
        memset(&ServerAddr, 0, sizeof(ServerAddr));
        ServerAddr.sin_family = AF_INET;
        ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        ServerAddr.sin_port = htons(atoi(servers[i].port.c_str()));
        if (bind(servers[i].socket_id, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)))
        {
            std::cout << "Error In Bind Server\n";
            exit(1);
        }
        std::cout << "Binding socket to local address...\n";
        // 3- Listen to the Clients connection request
        if (listen(servers[i].socket_id, 128))
        {
            std::cout << "No listen\n";
            exit(1);
        }
        std::cout << "Listening...\n";
        i++;
    }
    connection(servers);
}
