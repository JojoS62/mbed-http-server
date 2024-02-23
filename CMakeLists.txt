add_library(mbed-http-server STATIC)

target_include_directories(mbed-http-server
    INTERFACE
        ./source
)

target_sources(mbed-http-server
    PRIVATE
        source/ClientConnection.cpp
        source/HttpServer.cpp
        http_parser/http_parser.c    
)

target_link_libraries(mbed-http-server 
    PUBLIC 
        mbed-rtos-flags
        mbed-netsocket
)

