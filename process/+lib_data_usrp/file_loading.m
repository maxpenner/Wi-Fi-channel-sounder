function [complex_samples, n_files] = file_loading(n_channels, data_type_re_im, n_samples)

    % variables, must be exactly the same as in c++ code
    iq_file_param.folderpath = "../data";
    iq_file_param.data_type = data_type_re_im;
    iq_file_param.n_rx_channels = n_channels;
    iq_file_param.ch_measurement_per_sec = 1;           % old parameter, set to 1
    iq_file_param.ch_measurement_len = n_samples;
    iq_file_param.ch_measurement_save_period_sec = 1;   % old parameter, set to 1

    % first find all recorded files
    [filenames, n_files] = lib_util.get_all_filenames(iq_file_param.folderpath);
    
    % if we have no file, we cannot load anything
    if n_files == 0
        complex_samples = [];
        return;
	% It should not be more than one file.
    elseif n_files > 1
        complex_samples = [];
        return;
    end

    % create vector of elements, one element per file
    ch_meas_files(n_files,1) = lib_data_usrp.measurement_file();
    for n=1:n_files
        ch_meas_files(n) = lib_data_usrp.measurement_file(filenames(n), iq_file_param);
    end

    % load last file is there is more than one
    complex_samples = ch_meas_files(end).complex_samples;
end
