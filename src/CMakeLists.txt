add_executable(sockettapd
  sockettapd.cxx
  Application.cxx
  ConfigSessionDecoder.cxx
  STDecoder.cxx
  Sockettapd.cxx
  UUID.cxx
  OpencodeSessionID.cxx
  utils.cxx
)

target_include_directories(sockettapd
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sockettapd
  PRIVATE
    ${AICXX_OBJECTS_LIST}
    enchantum::enchantum
)
