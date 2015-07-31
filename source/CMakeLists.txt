set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -pedantic")

add_executable(ntfsrec
    ntfsrec_utility.h
    ntfsrec_utility.c
    
    ntfsrec_command.h
    ntfsrec_command.c
    ntfsrec_command_ls.c
    ntfsrec_command_cd.c
    ntfsrec_command_cp.c
    
    ntfs_reader.h
    ntfs_reader.c

    ntfsrec.h
    ntfsrec.c
)

target_link_libraries(ntfsrec ntfs-3g)

install(TARGETS ntfsrec RUNTIME DESTINATION bin)