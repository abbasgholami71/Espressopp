add_test(MTSAdResS ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_MTSAdResS.py)
set_tests_properties(MTSAdResS PROPERTIES ENVIRONMENT "${ESP_PY_ENV}")
