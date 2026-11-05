add_library(huenicorn_updater STATIC Release.cpp Platform.cpp UpdateController.cpp)
target_link_libraries(huenicorn_updater PUBLIC Qt6::Core Qt6::Network)
