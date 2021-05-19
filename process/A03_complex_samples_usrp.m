function [complex_samples, samp_rate_complex_samples, gain_used] = A03_complex_samples_usrp(samp_rate, n_channels, data_type_re_im, center_freq, run_id, n_samples)

    % wait time for C++-program to save binary iq samples file after sending udp instruction
    wait_time_cpp_file_save_sec = n_samples/samp_rate + 3.0;
    
    % agc
    agc_gain_default = 40;      % agc will use this defautl value to record samples, based on these samples it will adjust the gains
    agc_equal_gain = false;     % do we use the same gain for each usrp channel (true) or do we optimize for each channel (false)?
        
    % our default gain before running the agc
    gain_used = repmat(agc_gain_default, n_channels, 1);        

    % run agc
    gain_change = lib_data_usrp.agc(samp_rate, n_channels, data_type_re_im, center_freq, run_id, agc_gain_default);
    
    % sanity check
    if isempty(gain_change) == true
        fprintf('A03: AGC failed.\n');
        complex_samples = [];
        samp_rate_complex_samples = [];
        gain_used = [];
        return;
    end
    
    % adjust gain
    gain_used = gain_used + gain_change;

    % clip gain to integer values between 0dB and 60dB
    for ch=1:1:n_channels
        gain_used(ch) = floor(gain_used(ch));
        gain_used(ch) = min(60, gain_used(ch));
        gain_used(ch) = max(0, gain_used(ch));
    end

    % for equal gain we use the minimum gain to avoid clipping at all channels
    if agc_equal_gain == true
        gain_used = repmat(min(gain_change), n_channels, 1);
    end            

    % record samples with USRP, IQ samples are written to binary file
    fprintf('A03: Starting recording via UDP.\n');
    file_id = 1e6 + run_id;	% 1e6 is an arbitrary offset
    lib_data_usrp.udp_cmd(file_id, center_freq, n_samples, gain_used, false);

    pause(wait_time_cpp_file_save_sec);

    % C++ program should have created one file. Load it, extract samples and then delete it.
    % Should actually never throw errors, but theroretically it could.
    %   - IQ samples file is not completely written by C++ program, we load prematurely and therefore the number of bytes is not a multiple of the item size
    %   - we detect a file, but just before loading it somehow gets deleted
    % In case of an error best idea is to delete all binary IQ-samples files if there are any.
    try
        [complex_samples, n_files] = lib_data_usrp.file_loading(n_channels, data_type_re_im, n_samples);
        lib_util.clear_directory("../data/");
    catch
        lib_util.clear_directory("../data/");
        disp('A03: Function file_loading_deleting failed. Content of WiFi6/data/ deleted.');
        
        complex_samples = [];
        samp_rate_complex_samples = [];
        gain_used = [];
        return;
    end            

    % loading did not fail, there was no file
    if n_files == 0
        disp('A03: No files found.');
        
        complex_samples = [];
        samp_rate_complex_samples = [];
        gain_used = [];
        return;
        
    % loading did not fail, but there is more than one file
    elseif n_files > 1
        lib_util.clear_directory("../data/");
        disp('A03: More than one file found. Content of WiFi6/data/ deleted.');
        
        complex_samples = [];
        samp_rate_complex_samples = [];
        gain_used = [];
        return;
        
    end

    % we don't resamples, the wifi analyzer does that for us
    samp_rate_complex_samples = samp_rate;
end
