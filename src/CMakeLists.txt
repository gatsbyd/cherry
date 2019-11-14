###################################################################
#SRC CMAKELISTS
###################################################################

file(GLOB_RECURSE srcs ${PROJECT_SOURCE_DIR}/src/*.cpp ${PROJECT_SOURCE_DIR}/src/*.c ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB_RECURSE headers ${PROJECT_SOURCE_DIR}/src/*.h)

message (STATUS "srcs=" "${srcs}")
add_library(cherry STATIC ${srcs})
target_link_libraries(cherry melon)
target_link_libraries(cherry ${CMAKE_DL_LIBS})   #dlsym
target_link_libraries(cherry ${PROTOBUF_LIBRARIES}) #protobuf
target_link_libraries(cherry ${ZLIB_LIBRARIES})  #zlib


install(TARGETS cherry
        ARCHIVE DESTINATION /usr/lib
        LIBRARY DESTINATION /usr/lib)
install(FILES ${headers}
        DESTINATION /usr/local/include/cherry)
