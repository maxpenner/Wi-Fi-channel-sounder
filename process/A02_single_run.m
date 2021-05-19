function [] = A02_single_run(ext_sys_param, run_id)
    
    % extract external parameters
    samp_rate               = ext_sys_param.samp_rate;
    n_channels              = ext_sys_param.n_channels;
    data_type_re_im         = ext_sys_param.data_type_re_im;
    center_freq             = ext_sys_param.center_freq;
    bandwidth_vec           = ext_sys_param.bandwidth_vec;
    
    % we try to extact wifi packets, but not indefinitely
    n_max_try = 5;
    
    % how many samples do we record per try?
    n_samples = 100e6;
    
    mode = "complex_samples_usrp";
    %mode = "complex_samples_usrp_old";
    %mode = "complex_samples_simulation";
    
    n_min_wifi_packets = 10;                % how many wifi packets do we need until we are finished?
    analyzers = [];                         % here we collect found and decoded wifi analyzers
    
    % main loop
    for try_id=1:1:n_max_try
        
        fprintf('\nTry id %d\n', try_id);

        % record new samples with usrp
        if strcmp(mode, "complex_samples_usrp") == true

            % let usrp record new samples
            fprintf('Starting recording new samples.\n');
            [complex_samples, samp_rate_complex_samples, gain_used] = A03_complex_samples_usrp(samp_rate, n_channels, data_type_re_im, center_freq, run_id, n_samples);

        % load prerecorded samples
        elseif strcmp(mode, "complex_samples_usrp_old") == true

            % load old samples
            fprintf('Starting loading old samples.\n');
            [complex_samples, samp_rate_complex_samples, gain_used] = A04_complex_samples_usrp_old(n_channels, data_type_re_im, n_samples);           

        % generate samples within matlab
        elseif strcmp(mode, "complex_samples_simulation") == true

            % Generate new simulated data in matlab, get the new sampling rate.
            % Not fully implemented yet.
            fprintf('Starting simulating samples in matlab.\n');
            [complex_samples, samp_rate_complex_samples, gain_used] = A05_complex_samples_simulation(n_channels, n_samples, bandwidth_vec);

        else
            error('Unknown mode %s\n.', mode);
        end

        % error handling
        [check_n_samples, check_n_channels] = size(complex_samples);
        if check_n_samples ~= n_samples
            fprintf('Incorrect number of complex samples, is %d, should be %d. Next try.\n', check_n_samples, n_samples);
            pause(1.0);
            continue;
        elseif check_n_channels ~= n_channels
            fprintf('Incorrect number of rx channels, is %d, should be %d. Next try.\n', check_n_channels, n_channels);
            pause(1.0);
            continue;
        end

        % debugging: show samples in time domain
        A06_show_samples(complex_samples, samp_rate_complex_samples);
        pause(1.0);

        % iterate over each possible channel bandwidth
        for j=1:1:numel(bandwidth_vec)

            bandwidth = bandwidth_vec(j);

            % sanity check
            if samp_rate_complex_samples < bandwidth
                error('Sampling rate of complex samples smaller than bandwidth of wifi signal.');
            end

            % we need the sampling rate as a string for matlab
            if bandwidth == 20e6
                bandwidth_string = 'CBW20';
            elseif bandwidth == 40e6
                bandwidth_string = 'CBW40';
            elseif bandwidth == 80e6
                bandwidth_string = 'CBW80';
            elseif bandwidth == 160e6
                bandwidth_string = 'CBW160';
            else
                error('Unknown bandwidth %d.', bandwidth);
            end

            % Find, decode and extract wifi frames.
            % This function catches possible thrown error internally.
            fprintf('Processing %d million complex samples on %d channels.\n', size(complex_samples,1)/1e6, size(complex_samples,2));
            analyzers_local = A07_processing_multi_core(complex_samples, samp_rate_complex_samples, bandwidth_string);
            
            % append the gain of the usrp to each analyzer
            for ii=1:numel(analyzers_local)
                analyzers_local{ii}.usrp_gain_used = gain_used;
            end
            % append the gain of the usrp to each analyzer
            for ii=1:numel(analyzers_local)
                analyzers_local{ii}.usrp_gain_used = gain_used;
            end

            % append new analyzers
            if numel(analyzers_local) > 0
                analyzers = [analyzers; analyzers_local];
            end
        end

        % determine how many valid packets we currently have
        n_packets_valid = A09_MAC_filter(analyzers);

        % if we have found enough wifi packets, abort execution here
        if n_packets_valid < n_min_wifi_packets
            fprintf('Not enough wifi packets found, currently %d. Next try.\n', n_packets_valid);
        else
            disp('Enough wifi packets found.');
            break;
        end
    end
    
    % We save the results in two forms:
    %
    %   1. As a cell array containing all analyzer. When loading the file we can use the analyzer functions.
    %   2. As a cell containing only the wifi packets. This way we can load the structures in Python.
    
    % 1
    
    % create one file with all relevant data and save it
    meta_and_analyzers.ext_sys_param    	= ext_sys_param;
    meta_and_analyzers.run_id            	= run_id;
    meta_and_analyzers.n_max_try           	= n_max_try;
    meta_and_analyzers.n_samples         	= n_samples;
    meta_and_analyzers.mode                	= mode;
    meta_and_analyzers.n_min_wifi_packets  	= n_min_wifi_packets;
    meta_and_analyzers.try_idx          	= try_id;
    meta_and_analyzers.analyzers          	= analyzers;
    
    % determine how many packets we have currently
    n_packets_total = numel(lib_util.get_wifi_packets(analyzers));

    % save found wifi packets in .mat file
    out_filename = strcat(sprintf('%08d', run_id), "_", datestr(datetime('now'), 'yyyy_mm_dd_HH_MM_SS'), "_Number_of_packets_", sprintf('%06d', n_packets_total));
    save(fullfile("data/", out_filename), 'meta_and_analyzers');
    
    % 2
    
    % extract packets
    packets = lib_util.get_wifi_packets(analyzers);
    
    % create one file with all relevant data and save it
    meta_and_packets.ext_sys_param    	= ext_sys_param;
    meta_and_packets.run_id            	= run_id;
    meta_and_packets.n_max_try          = n_max_try;
    meta_and_packets.n_samples         	= n_samples;
    meta_and_packets.mode               = mode;
    meta_and_packets.n_min_wifi_packets = n_min_wifi_packets;
    meta_and_packets.try_idx            = try_id;
    meta_and_packets.packets            = packets;
    
    % determine how many packets we have
    n_packets_total = numel(packets);

    % save found wifi packets in .mat file
    out_filename = strcat(sprintf('%08d', run_id), "_", datestr(datetime('now'), 'yyyy_mm_dd_HH_MM_SS'), "_Number_of_packets_", sprintf('%06d', n_packets_total), "_packets");
    save(fullfile("data/", out_filename), 'meta_and_packets');
end
