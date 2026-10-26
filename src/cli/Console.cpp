#include "Console.h"
#include "CommandParser.h"
#include "../config/Config.h"
#include "../report/ReportUtil.h"
#include <iostream>
#include <stdexcept>

Console::Console() {}

Console::~Console() {
    if (scheduler_) {
        scheduler_->stop();
    }
}

void Console::printBanner() const {
    std::cout << R"(
   _____ _____  ____  _____  ______  _______     __
  / ____/ ____|/ __ \|  __ \|  ____|/ ____\ \   / /
 | |   | (___ | |  | | |__) | |__  | (___  \ \_/ /
 | |    \___ \| |  | |  ___/|  __|  \___ \  \   /
 | |___ ____) | |__| | |    | |____ ____) |  | |
  \_____|_____/ \____/|_|    |______|_____/   |_|
)" << std::endl;
    std::cout << "Hello, Welcome to CSOPESY commandline!" << std::endl;
    std::cout << "Type 'initialize' to start or 'exit' to quit." << std::endl;
    std::cout << std::endl;
}

void Console::run() {
    printBanner();

    std::string input;
    while (true) {
        std::cout << "root:\\> ";
        if (!std::getline(std::cin, input)) {
            break;  // EOF
        }

        if (input.empty()) continue;

        handleCommand(input);
    }
}

void Console::handleCommand(const std::string& input) {
    auto cmd = CommandParser::parse(input);

    if (cmd.command == "exit") {
        if (scheduler_) {
            scheduler_->stop();
        }
        std::cout << "Exiting CSOPESY Emulator. Goodbye!" << std::endl;
        exit(0);
    }

    if (!initialized_) {
        if (cmd.command == "initialize") {
            try {
                Config config = loadConfig("config.txt");
                scheduler_ = std::make_unique<Scheduler>(config);
                screenManager_ = std::make_unique<ScreenManager>(scheduler_.get());
                scheduler_->start();
                initialized_ = true;
                std::cout << "Initialized successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Initialization failed: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Error: Please run 'initialize' first before using other commands." << std::endl;
        }
        return;
    }

    // Post-initialize commands
    if (cmd.command == "initialize") {
        std::cout << "System is already initialized." << std::endl;
    }
    else if (cmd.command == "screen") {
        if (cmd.args.empty()) {
            std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << std::endl;
            return;
        }

        if (cmd.args[0] == "-s" && cmd.args.size() >= 2) {
            screenManager_->createScreen(cmd.args[1]);
        }
        else if (cmd.args[0] == "-r" && cmd.args.size() >= 2) {
            screenManager_->reattachScreen(cmd.args[1]);
        }
        else if (cmd.args[0] == "-ls") {
            screenManager_->listScreens();
        }
        else {
            std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls" << std::endl;
        }
    }
    else if (cmd.command == "scheduler-start") {
        scheduler_->startBatchGeneration();
        std::cout << "Scheduler batch generation started." << std::endl;
    }
    else if (cmd.command == "scheduler-stop") {
        scheduler_->stopBatchGeneration();
        std::cout << "Scheduler batch generation stopped." << std::endl;
    }
    else if (cmd.command == "report-util") {
        ReportUtil::writeToFile(scheduler_.get());
    }
    else {
        std::cout << "Unrecognized command: " << cmd.command << std::endl;
    }
}
