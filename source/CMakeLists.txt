set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -pedantic")

add_executable(ntfsrec
    ntfs_command.h
    ntfs_command.c

    ntfs_reader.h
    ntfs_reader.c

    ntfsrec.h
    ntfsrec.c
)

target_link_libraries(ntfsrec ntfs-3g)

install(TARGETS ntfsrec RUNTIME DESTINATION bin)