# SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
# SPDX-License-Identifier: BSD-3-Clause

add_library(webserver)
target_sources(webserver PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}/ApiServer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/HttpRequest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/HttpResponse.cpp
    ${CMAKE_CURRENT_LIST_DIR}/UrlMapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/WebServer.cpp
    )


target_include_directories(webserver PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    )

add_library(webserver-lwip)
target_sources(webserver-lwip PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/WebServer-lwip.cpp
    )

target_link_libraries(webserver-lwip
    webserver
    pico_stdlib
    )

target_include_directories(webserver-lwip PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${LWIP_DIR}/src/include
    )
