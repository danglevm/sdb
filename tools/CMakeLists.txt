# installs sdb to the default binary directory
include(GNUInstallDirs)


add_executable(sdb sdb.cpp)
# at runtime, installs sdb to the default binary directory
install(
    TARGETS sdb 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# PRIVATE only
target_link_libraries(sdb PRIVATE sdb::libsdb PkgConfig::readline fmt::fmt)