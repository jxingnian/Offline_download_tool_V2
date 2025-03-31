idf_component_register(SRCS     "main.c"      
                        "daplink/DAP_handle.c" 
                        "daplink/tcp_server.c" 
                        "daplink/usbip_server.c"  
                        "wifi/wifi_handle.c"
                        "wifi/http_server.c"
                        INCLUDE_DIRS "." "wifi" "daplink")
