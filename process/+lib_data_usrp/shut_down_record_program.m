function [] = shut_down_record_program()

    lib_data_usrp.udp_cmd(0, 0, 0, 0, true);

end

