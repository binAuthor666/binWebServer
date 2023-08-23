
ADD_EXECUTABLE(webserver main.cpp Config.cpp Webserver.cpp Http/HttpConn.cpp Log/Log.cpp SqlConnPool/SqlConnPool.cpp Timer/TimeWheel.cpp Timer/Utils.cpp)



target_link_libraries(webserver PUBLIC mysqlclient)
