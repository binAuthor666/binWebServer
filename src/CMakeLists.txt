
ADD_EXECUTABLE(webserver main.cpp Config.cpp Webserver.cpp Http/HttpConn.cpp Log/Log.cpp SqlConnPool/SqlConnPool.cpp Timer/LstTimer.cpp)



target_link_libraries(webserver PUBLIC mysqlclient)
