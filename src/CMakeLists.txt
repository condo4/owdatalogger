cmake_minimum_required (VERSION 2.6) 
project(OwDataLogger)



add_executable(owdatalogger owdatalogger.c dictionary.c iniparser.c) 

target_link_libraries (owdatalogger owcapi mysqlclient)