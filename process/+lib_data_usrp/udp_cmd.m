function [] = udp_cmd(file_id, center_freq, n_samples, gains, stop_execution)

    % We can send two different messages:
    %
    %   One message to trigger a new measurement:
    %
    %       16 Byte alphanumeric: New_Measurement_
    %       8 Byte uint32: File ID
    %           1 Byte alphanumeric delimiter: _
    %       4 Byte float: center frequency in MHz
    %           1 Byte alphanumeric delimiter: _
    %       10 Byte uint32: number of samples
    %           1 Byte alphanumeric delimiter: _
    %       4 Byte float: gain for each channel without delimiter
    %
    %   One message to shut down the c++ programm:
    %
    %       32 Bytes alphanumeric: End_Measurement_programm_now1234
    %
    
    if stop_execution == true
        text_sent = 'End_Measurement_programm_now1234';
    else
        
        % convert to unsigned integer
        u32_file_id     = uint32(file_id);
        u32_center_freq = uint32(center_freq);
        u32_n_samples 	= uint32(n_samples);
        u32_gains     	= uint32(gains);

        % convert to string
        str_file_id     = sprintf('%08d', u32_file_id);
        str_center_freq = sprintf('%04d', u32_center_freq);
        str_n_samples	= sprintf('%010d', u32_n_samples);
        str_gains    	= sprintf('%04d', u32_gains);

        % human readable string with all data
        text_sent = strcat('New_Measurement_', str_file_id, '_', str_center_freq, '_', str_n_samples, '_', str_gains);        
    end

    fixed_message_size = 64;

    % make sure the message has a length of fixed_message_size
    if numel(text_sent) > fixed_message_size
        error('Message sent to long with %d Byte.', numel(text_sent));
    else
        text_sent = strcat(text_sent, repelem('x', fixed_message_size-numel(text_sent)));
    end

    % All udp functionality.
    % Do not change to TCP, UDP nonconnected behaviour is required in case C++-program crashes.
    udps = dsp.UDPSender('RemoteIPPort',8888);
    dataSent = uint8(text_sent);
    udps(dataSent);
    release(udps);
end
