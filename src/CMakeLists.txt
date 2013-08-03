cmake_minimum_required (VERSION 2.6) 
project(OwDataLogger)

find_path(MYSQL_INCLUDE_DIR
  NAMES
    mysql/my_global.h
  PATH_SUFFIXES
    include
)

include_directories(${MYSQL_INCLUDE_DIR})

add_executable(owdatalogger owdatalogger.c dictionary.c iniparser.c) 

target_link_libraries (owdatalogger owcapi mysqlclient)
