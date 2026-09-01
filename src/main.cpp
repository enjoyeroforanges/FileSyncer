#include "../include/cli.h"
#include "../include/ssh.h"
#include "../include/sync.h"
#include <iostream>
#include <fstream>



int main(int argc, char* argv[]){
    auto args = parseArgs(argc, argv);
    if (args == std::nullopt){
        std::cout << "Usage:" << std::endl; // need to add later
    }
    switch (args -> command){
        case Command::sync: break;

        case Command::gethashes:{
            std::fstream inputFile(args->filePath, std::ios::in | std::ios::binary);
            
            if (!inputFile.is_open()){
                std::cout << "error opening file" << std::endl;
                return -1;
            }
            sendHashes(inputFile);
            
        }
    }
}