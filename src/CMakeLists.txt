include(GNUInstallDirs)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# export
install(
    EXPORT sdb-targets 
    FILE sdb-config.cmake  
    NAMESPACE sdb::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sdb
)


add_library(libsdb process.cpp pipe.cpp) # add the following source code to be compiled as a library
add_library(sdb::libsdb ALIAS libsdb) # use a namespaced library target and give it a new name

# Linux outputs library called lib<target_name>. We refine it to sdb
set_target_properties(
    libsdb
    PROPERTIES OUTPUT_NAME sdb
)

# any target that depends on libsdb will also be automatically compiled with C++17
# any target that links to libsdb must also use C++
target_compile_features(libsdb PUBLIC cxx_std_17) 

# any target linking against libsdb will not have access to headers inside sdb/src/include. Linking against: where to find said symbols
# avoid header pollution and only libsdb need it
# target_include_directories(
#     libsdb
#     PRIVATE include
#     PUBLIC ${CMAKE_SOURCE_DIR}/include # dependent targets should inherit this folder
# )

target_include_directories(
    libsdb
    PUBLIC
        $<INSTALL_INTERFACE:include> # if the user accesses the installed version, look for public header in include 
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> # if user uses a version in the build tree, use include directory inside the source tree
    PRIVATE   
        ${CMAKE_SOURCE_DIR}/src/include
)       

# export targets libsdb as sdbtargets called sdb-targets with information about where to install different components of the project
# marks libsdb to be exported as part of the sdb-target export set
# enables other projects to import libsdb later using find_package(sdb)
install(TARGETS libsdb
    EXPORT sdb-targets 
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)