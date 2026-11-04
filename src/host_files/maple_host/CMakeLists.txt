add_library(maple_host INTERFACE)
target_include_directories(maple_host INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(maple_host INTERFACE pico_stdlib hardware_pio hardware_dma psx-device)
target_sources(maple_host INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/MapleBus.cpp
        ${CMAKE_CURRENT_LIST_DIR}/MapleBus.h
        ${CMAKE_CURRENT_LIST_DIR}/maple_host.cpp
        ${CMAKE_CURRENT_LIST_DIR}/maple_host.h)
pico_generate_pio_header(maple_host ${CMAKE_CURRENT_LIST_DIR}/maple_in.pio)
pico_generate_pio_header(maple_host ${CMAKE_CURRENT_LIST_DIR}/maple_out.pio)
