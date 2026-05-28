add_executable(dbms_tests test_vector.cpp)
target_include_directories(dbms_tests PRIVATE ../src)
add_test(NAME dbms_tests COMMAND dbms_tests)

add_executable(test_core test_core.cpp)
target_include_directories(test_core PRIVATE ../src)
add_test(NAME test_core COMMAND test_core)

add_executable(test_storage test_storage.cpp)
target_include_directories(test_storage PRIVATE ../src)
add_test(NAME test_storage COMMAND test_storage)

add_executable(test_bptree test_bptree.cpp)
target_include_directories(test_bptree PRIVATE ../src)
add_test(NAME test_bptree COMMAND test_bptree)

add_executable(test_parser test_parser.cpp)
target_include_directories(test_parser PRIVATE ../src)
add_test(NAME test_parser COMMAND test_parser)

add_executable(test_executor test_executor.cpp)
target_include_directories(test_executor PRIVATE ../src)
add_test(NAME test_executor COMMAND test_executor)
