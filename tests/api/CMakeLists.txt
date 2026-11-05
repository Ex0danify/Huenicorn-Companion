add_executable(TestApiClient TestApiClient.cpp)
target_link_libraries(TestApiClient PRIVATE huenicorn_api test_support)
add_test(NAME ApiClient COMMAND TestApiClient)
