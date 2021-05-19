function [analyzer] = A08_processing(complex_samples, samp_rate_complex_samples, bandwidth_string)

    % if the matlab function throws an error, we just return an empty structure like we found no frames
    try
        analyzer = WaveformAnalyzer;
        process(analyzer, complex_samples, bandwidth_string, samp_rate_complex_samples);

        % we need to drastically reduce the size
        analyzer.FORMAT_filter_CRC_filter_SIZE_reduction();
    catch
        analyzer = [];
        disp('Non-Fatal Error: Function process() of WaveformAnalyzer threw error. Returning empty.');
    end 
end

