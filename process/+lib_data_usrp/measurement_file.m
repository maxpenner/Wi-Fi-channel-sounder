classdef measurement_file
    %MEASUREMENT_FILE Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        name
        folder
        date
        bytes
        full_filepath
        
        measurement_id
        file_id
        microseconds_since_epoch
        
        measurement_time
        
        iq_file_param_cpy
        
        complex_samples
    end
    
    methods
        function obj = measurement_file(file_struct, iq_file_param)
            if nargin == 0
                return;
            end
            obj.name    = file_struct.name;
            obj.folder  = file_struct.folder;
            obj.date    = file_struct.date;
            obj.bytes   = file_struct.bytes;
            
            obj.full_filepath = fullfile(obj.folder,obj.name);
            
            % extract all partis of the file name
            name_split = split(obj.name,'_');
            measurement_id_string           = name_split{2};
            file_id_string                  = name_split{3};
            microseconds_since_epoch_string	= name_split{4};
            
            % remove possible file extension
            [~, measurement_id_string, ~]        	= fileparts(measurement_id_string);
            [~, file_id_string, ~]                	= fileparts(file_id_string);
            [~, microseconds_since_epoch_string, ~]	= fileparts(microseconds_since_epoch_string);
            
            % convert to either double or uint64_t
            obj.measurement_id             	= str2double(measurement_id_string);
            obj.file_id                    	= str2double(file_id_string);
            obj.microseconds_since_epoch	= sscanf(microseconds_since_epoch_string, '%lu'); % source: https://de.mathworks.com/matlabcentral/answers/446084-conversion-from-unix-time-string-to-uint64
            
            % convert uint64_t to date string with microsecond precision
            obj.measurement_time = lib_util.timestring_from_uint64t_since_epoch(obj.microseconds_since_epoch);
            
            obj.iq_file_param_cpy = iq_file_param;
            
            obj.complex_samples = read_samples(obj);
        end
    end
end

function complex_samples = read_samples(obj)
    complex_samples = read_complex_binary(obj.full_filepath, obj.iq_file_param_cpy.data_type, 0, 0);

    % channels are concatenated
    n_complex_samples = numel(complex_samples);
    n_complex_samples_per_channel = n_complex_samples/obj.iq_file_param_cpy.n_rx_channels;

    % separate into channels
    complex_samples = reshape(complex_samples, n_complex_samples_per_channel, obj.iq_file_param_cpy.n_rx_channels);

    % sanity check
    len = obj.iq_file_param_cpy.ch_measurement_per_sec;
    len = len * obj.iq_file_param_cpy.ch_measurement_len;
    len = len * obj.iq_file_param_cpy.ch_measurement_save_period_sec;
    if len ~= numel(complex_samples(:,1))
        error('Incorrect number of samples per channel per saved file.');
    end 
end        
        
% source: https://github.com/UpYou/gnuradio-tools/blob/master/matlab/read_complex_binary.m
%
% Changes: Samples can be skipped, added data types.
function v = read_complex_binary(filename, data_type, count, skip_samples)

    % what is the size of the input data in bytes
    switch lower(data_type)
        case {'double', 'int64', 'uint64'}
            byte_of_data_type = 8;
        case {'float', 'single', 'int32', 'uint32'}
            byte_of_data_type = 4;
        case {'char', 'int16', 'uint16'}
            byte_of_data_type = 2;
        case {'logical', 'int8', 'uint8'}
            byte_of_data_type = 1;
    otherwise
        warning('JSimon:sizeof:BadClass', 'Class "%s" is not supported.', data_type);
        byte_of_data_type = NaN;
    end

    f = fopen(filename, 'rb');

    if (f < 0)

        error('ERROR: Cannot read file with path: %s', filename);

    else

        % check if skip samples and read samples are smaller than actual file size
        filebytes = dir(filename);

        % 0 indicates that we want the maximum file size
        if count == 0
            if mod(filebytes.bytes,byte_of_data_type) ~= 0
                error('ERROR: filesize in bytes not a multiple of item size');
            end
            count = filebytes.bytes/byte_of_data_type;
        end

        % 2 -> real and imag
        if((skip_samples + count)*byte_of_data_type > filebytes.bytes)
            fprintf('\nFilesize in bytes:            %d\n', filebytes.bytes);
            fprintf('\nSkip and count size in bytes: %d\n', (skip_samples + count)*2*byte_of_data_type);       
            error('ERROR: Skip and count are larger than file.');
        end 

        % skip complex samples
        if(skip_samples > 0)
            fseek(f, skip_samples*byte_of_data_type,'bof');
        end

        % read in bytes
        t = fread(f, [2, count], data_type);
        fclose (f);
        v = t(1,:) + t(2,:)*1i;
        [r,c] = size(v);
        v = reshape (v, c, r);      
    end
end