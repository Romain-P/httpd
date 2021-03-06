//
// Created by romain on 3/1/19.
//

#include "Console.h"
#include "Server.h"
#include "zia.h"
#include <iostream>

const std::unordered_map<std::string, bool(Console::*)(std::vector<std::string>)> Console::commands {
        { "exit", &Console::onExit },
        { "config", &Console::onConfig },
        { "start", &Console::onStart },
        { "stop", &Console::onStop },
        { "restart", &Console::onRestart },
        { "load", &Console::onLoad },
        { "unload", &Console::onUnload },
        { "loadall", &Console::onLoadAll },
        { "unloadall", &Console::onUnloadAll },
        { "list", &Console::onList },
        { "help", &Console::onHelp }
};

void Console::awaitCommands() {
    for (std::string line; std::getline(std::cin, line);) {
        if (line.empty()) continue;

        auto args = string_util::split(line, ' ');
        auto cmd = commands.find(args[0]);
        if (cmd != commands.end()) {
            if (((*this).*(cmd->second))(args)) return;
        } else
            pureinfo("[console]: command not found, type help for the list\n");
    }
    onExit();
}

bool Console::onExit(std::vector<std::string>) {
    info("exit request");
    if (server.network().thread().joinable()) {
        server.network().stop();
    }

    server.modules().unloadAll();
    return true;
}

bool Console::onConfig(std::vector<std::string>) {
    server.reloadConfig();
    info("config reloaded");
    return false;
}

bool Console::onStart(std::vector<std::string>) {
    if (server.network().thread().joinable())
        errors("tcp server already started");
    else
        server.network().start();
    return false;
}

bool Console::onStop(std::vector<std::string>) {
    if (!server.network().thread().joinable())
        errors("tcp server already stopped");
    else
        server.network().stop();
    return false;
}

bool Console::onRestart(std::vector<std::string>) {
    server.network().restart();
    info("tcp server restarted");
    return false;
}

bool Console::onLoad(std::vector<std::string> args) {
    if (args.size() < 3) {
        errors("expected arguments [module] and [priority]");
        return false;
    }
    std::string &path = args[1];
    ssizet priority = std::stoi(args[2]);

    server.modules().load(path, priority);
    return false;
}

bool Console::onUnload(std::vector<std::string> args) {
    if (args.size() < 2) {
        errors("missing argument [module]");
        return false;
    }
    server.modules().unload(args[1]);
    return false;
}

bool Console::onLoadAll(std::vector<std::string>) {
    server.modules().loadAll();
    return false;
}

bool Console::onUnloadAll(std::vector<std::string>) {
    server.modules().unloadAll();
    return false;
}

bool Console::onList(std::vector<std::string>) {
    pureinfo(server.modules().dumb());
    return false;
}

bool Console::onHelp(std::vector<std::string>) {
    pureinfo("[console]: command list\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n  %s:\t\t%s\n",
            "config                   ", "reloads the configuration file",
            "start                    ", "starts the tcp server",
            "stop                     ", "stops the tcp server",
            "restart                  ", "restarts the tcp server",
            "load [module] [priority] ", "loads a module",
            "unload [module]          ", "unloads a module",
            "loadall                  ", "loads all modules",
            "unloadall                ", "unloads all modules",
            "list                     ", "list all loaded modules",
            "exit                     ", "exits this program");
    return false;
}
