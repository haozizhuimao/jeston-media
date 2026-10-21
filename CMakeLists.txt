set(KIT_NAME "kit_ffmpeg_puller")
file(GLOB SRC_CURRENT ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
message("---------- Building ${KIT_NAME}  ----------")
add_library(${KIT_NAME} OBJECT ${SRC_CURRENT})

# 添加源文件
target_include_directories(${KIT_NAME} PUBLIC  
    ${CMAKE_CURRENT_SOURCE_DIR} 
    ${THIRD_PARTY_INCLUDE}/ffmpeg
)       

target_link_directories(${KIT_NAME} PUBLIC ${THIRD_PARTY_LIB}/ffmpeg)

target_link_libraries(${KIT_NAME} PUBLIC
    shared_dependencies
    kit_log
    avcodec
    avformat
    avutil
    swscale
    swresample
    avfilter
)


