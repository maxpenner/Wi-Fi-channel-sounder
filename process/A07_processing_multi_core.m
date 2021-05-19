function [analyzers] = A07_processing_multi_core(complex_samples, samp_rate_complex_samples, bandwidth_string)

    % number of samples per core for processing
    n_samples_per_tread = 2.5e6;
    
    % how many complex samples do we have?
    n_samples = size(complex_samples,1);
    
    % check how many runs we need
    n_runs = floor(n_samples/n_samples_per_tread);
    if n_runs < 1
        disp('We have to have at least one full run. Increase number of samples.');
        analyzers = [];
        return;
    end
    
    % slice input so rx_original does not become a broadcast variable
    input_sliced = cell(n_runs, 1);
    for i=1:1:n_runs
        start_idx = (i-1)*n_samples_per_tread+1;
        end_idx = i*n_samples_per_tread;        
        input_sliced(i) = {complex_samples(start_idx:end_idx, :)};
    end
    
    % output per run
    analyzers = cell(n_runs, 1);
    
    % process sliced complex_samples on multiple cores
    parfor i=1:n_runs
    %for i=1:n_runs
    
        analyzer_local = A08_processing(input_sliced{i}, samp_rate_complex_samples, bandwidth_string);
        
        % In A08_processing() we try to process a chunk of IQ samples.
        % Internally, Matlab's WaveformAnalyzer is called. From time to time the WaveformAnalyzer throws errors.
        % If that happens, analyzer_local is empty.
        % It will be removed below.
        if isempty(analyzer_local) == false
            analyzers(i) = {analyzer_local};
        end
    end
    
    % remove empty elements, indicating that slice did not contain any wifi frames
    analyzers = analyzers(~cellfun('isempty',analyzers));    
    
    % concatenate individual packets -> removes cell array
    %analyzers = vertcat(analyzers{:});
end

