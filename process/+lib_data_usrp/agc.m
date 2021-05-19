function [gain_change] = agc(samp_rate, n_channels, data_type_re_im, center_freq, run_id, gain_default)
    
    plot_debug = false;

    % how many samples to we record for our agc?
    n_samples = 25e6;
    
    % wait time for C++-program to save binary iq samples file after sending udp instruction
    wait_time_cpp_file_save_sec = n_samples/samp_rate + 3.0;

    % record samples with USRP, iq samples are written to binary file
    fprintf('AGC: Starting recording via UDP.\n');
    file_id = 1e6 + run_id;
    lib_data_usrp.udp_cmd(file_id, center_freq, n_samples, repmat(gain_default, n_channels, 1), false);

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
        disp('AGC: Function file_loading_deleting failed. Content of WiFi6/data/ deleted. Next try.');
        gain_change = [];
        return;
    end            

    % handle errors
    if n_files == 0
        disp('AGC: No files found.');
        gain_change = [];
        return;
    elseif n_files > 1
        disp('AGC: More than one file found. Content of WiFi6/data/ deleted.');
        gain_change = [];
        return;
    end
    
    % plot stats
    if plot_debug == true
        figure(1)
        clf()
    end
    
    % init
    gain_change = zeros(n_channels,1);

    % we need the gain for each channel
    for ch=1:1:n_channels

        complex_samples_ch = complex_samples(:,ch);

        % first take the absolute
        complex_samples_ch_abs = abs(complex_samples_ch);

        % apply a moving average filter
        complex_samples_ch_abs_movmean = movmean(complex_samples_ch_abs, 1000);

        % delete values below a threshold, considered noise
        complex_samples_ch_abs_movmean_no_noise = complex_samples_ch_abs_movmean;
        noise_threshold = 0.01;
        complex_samples_ch_abs_movmean_no_noise(complex_samples_ch_abs_movmean_no_noise < noise_threshold) = [];

        % create a histogram of all values
        [N, edges] = histcounts(complex_samples_ch_abs_movmean_no_noise,100);
        bins_center = (edges(1:end-1) + edges(2:end))/2;

        % find peaks
        thres = 0.09;       % anything above this comes from the AP @ 40dB default gain
        bins_center_thres = bins_center;
        bins_center_thres(bins_center_thres > thres) = [];
        N_thres = N(1:numel(bins_center_thres));
        [pk,lk] = findpeaks(N_thres, bins_center_thres, 'NPeaks', 6, 'SortStr', 'descend');
        
%         % OPTIONA A: pick the peak with the largest location
%         max_abs = max(lk);
        
        % OPTION B: pick the location of the largest peak
        max_abs = lk(1);

        if plot_debug == true
            subplot(n_channels*2,1, 2*ch-1)
            plot(complex_samples_ch_abs,'b');
            hold on
            plot(complex_samples_ch_abs_movmean,'r');
            plot(complex_samples_ch_abs_movmean_no_noise,'g');

            title("AGC Samples of Channel Index " + num2str(ch-1));
            legend('abs', 'abs + mov avg', 'abs + mov avg - noise');
            xlabel('Sample Index');
            ylabel('Amplitude');
            grid on          

            subplot(n_channels*2,1,2*ch)
            plot(bins_center, N);
            %semilogy(bins_center, N);
            hold on
            plot(lk,pk,'x')
            xline(max_abs);
            xline(noise_threshold, 'g');

            title("AGC Histogram of Channel Index " + num2str(ch-1));
            xlabel('Amplitude');
            ylabel('Count');
            grid on
        end

        % Our target amplitude.
        % OFDM samples are zero mean gaussian distributed, so the absolute is a folded gaussian distribution.
        max_abs_target = 0.15;

        % by how much do we need to increase or decrease the amplitude
        amp = max_abs_target/max_abs;

        % what is the required gain increase?
        gain_change(ch) = 10*log10(amp^2);
    end
end
