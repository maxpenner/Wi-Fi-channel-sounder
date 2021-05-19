function [complex_samples, samp_rate_complex_samples, gain_used] = A04_complex_samples_usrp_old(n_channels, data_type_re_im, n_samples)

    % iterate over variables
    persistent n
    if isempty(n) == true
        n = 0;
    end
    n=n+1;
    
    % we have to know with what sampling rate the files were recorded
    samp_rate_complex_samples = 100e6;
    
    % knowing the gain is no required
    gain_used = zeros(n_channels,1);
    
    % variables, must be exactly the same as in c++ code
    iq_file_param.folderpath = "../data_old";
    iq_file_param.data_type = data_type_re_im;
    iq_file_param.n_rx_channels = n_channels;
    iq_file_param.ch_measurement_per_sec = 1;           % old parameter, set to 1
    iq_file_param.ch_measurement_len = n_samples;
    iq_file_param.ch_measurement_save_period_sec = 1;   % old parameter, set to 1

    % first find all recorded files
    [filenames, n_files] = lib_util.get_all_filenames(iq_file_param.folderpath);
    
    % if we have no file, we cannot load anything
    if n_files == 0
        disp('There is no data with old recorded usrp data.');
        
        complex_samples = [];
        samp_rate_complex_samples = [];
        gain_used = [];
        return;        
    end

    % create vector of elements, one element per file
    ch_meas_files = lib_data_usrp.measurement_file(filenames(n), iq_file_param);
    
    % we iterate over the same files
    n = mod(n,n_files);

    % pick a random file and get samples
    complex_samples = ch_meas_files.complex_samples;
end

