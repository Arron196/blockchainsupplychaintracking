add_executable(test_packet_sign
    test_packet_sign.c
)

target_link_libraries(test_packet_sign
    PRIVATE
        fw_app
)

add_test(
    NAME packet_sign_canonicalization
    COMMAND test_packet_sign
)
