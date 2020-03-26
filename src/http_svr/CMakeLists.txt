# CMake 最低版本号要求
cmake_minimum_required (VERSION 2.8)
# 项目信息
project (HttpSvr)

set(CMAKE_BUILD_TYPE "Debug")

# 将名称保存到 变量
set(DIR_HTTPSVR .)
set(DIR_FRAME ../frame)

include_directories(${DIR_FRAME}/)
include_directories(${DIR_HTTPSVR}/)
include_directories(${DIR_FRAME}/thread_pool/)
include_directories(${DIR_FRAME}/httpd/)
include_directories(${DIR_FRAME}/../3rd/hredis/)
include_directories(${DIR_HTTPSVR}/../3rd/protobuf3.9/include)
include_directories(${DIR_HTTPSVR}/../3rd/zlib/include)
include_directories(${DIR_HTTPSVR}/game_room/)

#查找当前目录下的所有源文件
#并将名称保存到DIR_FRAME变量
aux_source_directory(${DIR_FRAME} FRAME)
aux_source_directory(${DIR_FRAME}/thread_pool/ THREAD_POOL)
aux_source_directory(${DIR_FRAME}/httpd HTTPD)
aux_source_directory(${DIR_FRAME}/../3rd/hredis/ REDIS)
aux_source_directory(${DIR_HTTPSVR} HTTPSVR)
aux_source_directory(${DIR_HTTPSVR}/game_room/ ROOM)


list(REMOVE_ITEM FRAME ${DIR_FRAME}/test.cpp)

MESSAGE(${DIR_FRAME})
MESSAGE( ${THREAD_POOL} )
MESSAGE( "----------------------------------------------------\n" ${HTTPD} "|||||" ${DIR_FRAME}/httpd/)


#添加动态库目录
#link_libraries(${PROTOLIB})
# find_library(PROTOLIB protobuf /home/wrongmean/app/server/src/3rd/protobuf3.9/lib/)
# MESSAGE("----------"  ${PROTOLIB})
link_libraries("/home/wrongmean/app/server/src/3rd/protobuf3.9/lib/libprotobuf.so")
link_libraries("/home/wrongmean/app/server/src/3rd/zlib/lib/")

# 指定生成目标
add_executable(HttpSvr  ${HTTPSVR} ${ROOM} ${HTTPD} ${FRAME} ${THREAD_POOL} ${REDIS})
target_link_libraries(HttpSvr /home/wrongmean/app/server/src/3rd/protobuf3.9/lib/libprotobuf.so) 
target_link_libraries(HttpSvr /home/wrongmean/app/server/src/3rd/zlib/lib/libz.so) 
# 添加链接库
target_link_libraries(HttpSvr pthread )



