set(TARGET_NAME "starbytes-${STARBYTES_PROJECT_VERSION}")
add_library(${TARGET_NAME} SHARED "starbytes.cpp" "include/starbytes.h")
set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")