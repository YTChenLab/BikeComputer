# armclang compilation may use an existing GNU/LLVM ELF archiver for host checks.
# Firmware linking still belongs to the board project and its AC6 linker.
get_filename_component(UI_ARCHIVER_NAME "${CMAKE_AR}" NAME)
if(UI_ARCHIVER_NAME MATCHES "^(ar|llvm-ar)(\\.exe)?$")
    foreach(lang C CXX ASM)
        set(CMAKE_${lang}_CREATE_STATIC_LIBRARY "<CMAKE_AR> qc <TARGET> <OBJECTS>")
        set(CMAKE_${lang}_ARCHIVE_CREATE "<CMAKE_AR> qc <TARGET> <OBJECTS>")
        set(CMAKE_${lang}_ARCHIVE_APPEND "<CMAKE_AR> q <TARGET> <OBJECTS>")
        set(CMAKE_${lang}_ARCHIVE_FINISH "<CMAKE_AR> s <TARGET>")
        set(CMAKE_${lang}_RESPONSE_FILE_LINK_FLAG "@")
    endforeach()
endif()
