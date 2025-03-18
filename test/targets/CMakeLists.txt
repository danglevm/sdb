add_executable(run_endlessly run_endlessly.cpp)
add_executable(end_immediately end_immediately.cpp)

add_executable(reg_write reg_write.s)
# positions to the code in memory in a flexible way
target_compile_options(reg_write PRIVATE -pie)

add_executable(reg_read reg_read.s)
target_compile_options(reg_read PRIVATE -pie)

add_executable(hello_sdb hello_sdb.cpp)