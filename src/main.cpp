/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lmoheyma <lmoheyma@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/03/27 17:38:19 by lmoheyma          #+#    #+#             */
/*   Updated: 2024/04/15 00:10:54 by lmoheyma         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "main.hpp"

void freeAll(std::vector<WebServ *> servers)
{
    for (std::vector<WebServ *>::iterator it = servers.begin(); it != servers.end(); it++)
    {
        WebServ* server = *it;
        ServerConfig config = server->getConfig();
        for (std::map<std::string, RouteConfig>::iterator it = config.getRoutes().begin(); it != config.getRoutes().end(); it++)
        {
            RouteConfig route = it->second;
            route.getLimitExceptAccepted().clear();
            route.getReturnCodes().clear();
        }
        config.getErrorsPages().clear();
        config.getRoutes().clear();
        delete server;
    }
}

int	main(int argc, char **argv)
{
	std::string path;
    if (argc == 1)
        path = "./conf/webserv.conf";
    else if (argc == 2)
        path = argv[1];
    else
    {
        std::cerr << "Too much args" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << BOLDCYAN << "[" << getDisplayDate() << "] " << BOLDMAGENTA 
            << "# configuration file " << path << RESET << std::endl;
    std::vector<ServerConfig> server_configs;
    getConfig(path, server_configs);
    for (std::vector<ServerConfig>::iterator it = server_configs.begin(); it != server_configs.end(); it++)
    {
        ServerConfig config = *it;
        std::cout << BOLDCYAN << "[" << getDisplayDate() << "] " << BOLDGREEN 
            << "listening on port " << config.getPort() << RESET << std::endl;
    }

    for (std::vector<ServerConfig>::iterator it = server_configs.begin(); it != server_configs.end(); it++)
    {
        ServerConfig config = *it;
        for (std::vector<ServerConfig>::iterator it2 = server_configs.begin(); it2 != server_configs.end(); it2++)
        {
            ServerConfig config2 = *it2;
            if (it == it2)
                continue;
            if (config.getServerName() == config2.getServerName() && !config.getServerName().empty() && !config2.getServerName().empty())
            {
                std::cerr << "Error same hostnames." << std::endl;
                exit(EXIT_FAILURE);
            }

        }
    }
    int epoll_fd = epoll_create(1);
    if (epoll_fd == -1) {
        std::cerr << "Error creating epoll: " << std::strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<WebServ *> servers;
    for (size_t i = 0; i < server_configs.size(); i++)
    {
        WebServ *new_serv = new WebServ(server_configs[i], epoll_fd, servers);
        servers.push_back(new_serv);
    }

    int num_events = 0;
    std::map<int, Socket> client_fds;
    std::vector<epoll_event> events(20);
    while (true)
    {
        num_events = epoll_wait(epoll_fd, events.data(), 20, -1);
        if (num_events == -1) {
            std::cerr << "Error in epoll_wait: " << std::strerror(errno) << std::endl;
            return EXIT_FAILURE;
        }

        clock_t time_now = clock();
        bool have_timeout = false;

        for (std::map<int, Socket>::iterator it = client_fds.begin(); it != client_fds.end(); it++)
        {
            time_now = clock();
            double diff_time = static_cast<double>(time_now - it->second.getTime()) / CLOCKS_PER_SEC;
            if (diff_time > 5.0)
            {
                client_fds.erase(it->first);
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, it->first, NULL) < 0)
                {
                    std::cerr << "Error Deleting EPOLL : " << std::strerror(errno) << std::endl;
                    exit(EXIT_FAILURE);
                }
                close(it->first);
                have_timeout = true;
                break;
            }
        }
        if (have_timeout)
            continue;

        if (num_events < 1)
            continue;
        int valread;
        for (int i = 0; i < num_events; ++i) {
            int client_fd;
            std::vector<WebServ *>::iterator it;
            for (it = servers.begin(); it != servers.end(); it++)
            {
                WebServ* server = *it;
                int server_fd = server->getServerFd();
                if (events[i].data.fd == server_fd) {
                    server->setEpollFd(epoll_fd);
                    client_fd = server->create_client();
                    client_fds.insert(std::make_pair(client_fd, Socket(client_fd, server_fd)));
                    break;
                }
            }
            if (it == servers.end())
                client_fd = events[i].data.fd;
            if ((events[i].events & EPOLLIN))
            {
                    char buffer[1024] = {0};
                    valread = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
                    if (valread < 1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            continue;
                        }
                        std::cerr << "Error Reading : " << std::endl;
                        client_fds.erase(client_fd);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) < 0)
                        {
                            std::cerr << "Error Deleting EPOLL : " << std::strerror(errno) << std::endl;
                            exit(EXIT_FAILURE);
                        }
                        close(client_fd);
                        continue;
                    }
                    client_fds[client_fd].addToBuffer(buffer, valread);
            }
            if (!(events[i].events & EPOLLIN) && (events[i].events & EPOLLOUT) && valread > 0)
            {
                std::vector<char> temp = client_fds[client_fd].getBufferVector();
                std::string str(temp.begin(), temp.end());
                Request req(str);
                std::string response;
                for (it = servers.begin(); it != servers.end(); it++)
                {
                    WebServ* server = *it;
                    if (client_fds[client_fd].getServerFd() == server->getServerFd())
                    {
                        ServerConfig config = server->getConfig();
                        RouteConfig route;
                        
                        if (config.getRoutes().find(req.getPath()) != config.getRoutes().end())
                            route = config.getRoutes().find(req.getPath())->second;
                        else
                            route = config.getRoutes().find("/")->second;
                        std::cout << BOLDCYAN << "[" << getDisplayDate() << "] " << BOLDGREEN << "server : "
                            << BOLDWHITE << "<< " << BOLDGREEN << "[method: " << req.getMethod() 
                            << "] [target: " << req.getPath() << "] [server fd: " 
                            << client_fds[client_fd].getServerFd() << "] [location: " << route.getRoot() << "]" << RESET << std::endl;
                        if (req.getMethod() == "GET" && (std::find(route.getLimitExceptAccepted().begin(), route.getLimitExceptAccepted().end(), req.getMethod()) != route.getLimitExceptAccepted().end()))
                            response = handleGET(req, route, config);
                        else if (req.getMethod() == "POST" && (std::find(route.getLimitExceptAccepted().begin(), route.getLimitExceptAccepted().end(), req.getMethod()) != route.getLimitExceptAccepted().end()))
                            response = handlePOST(req, route, config);
                        else if (req.getMethod() == "DELETE" && (std::find(route.getLimitExceptAccepted().begin(), route.getLimitExceptAccepted().end(), req.getMethod()) != route.getLimitExceptAccepted().end()))
                            response = handleDELETE(req, route, config);
                        else
                        {
                            Response rep;
                            response = getErrorsPages("501", route, config, rep);
                        }
                        break;
                    }
                }
                if (send(client_fd, response.c_str(), response.length(), 0) != static_cast<long int>(response.length()))
                {
                    std::cerr << "Error Sending" << std::endl;
                }
                client_fds.erase(client_fd);
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) < 0)
                {
                    std::cerr << "Error Deleting EPOLL : " << std::strerror(errno) << std::endl;
                    exit(EXIT_FAILURE);
                }
                std::cout << BOLDCYAN << "[" << getDisplayDate() << "] " << BOLDGREEN
                        << "server : connection closed " << RESET << std::endl;
                close(client_fd);
            }
        }
    }
	return (0);
}