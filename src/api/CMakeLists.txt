add_library(huenicorn_api STATIC ApiClient.cpp)
target_link_libraries(huenicorn_api PUBLIC Qt6::Core Qt6::Network)
