clear all;
close all;
clc;

% this is a test function which we use to check the results

% get all files with wifi packets
[filenames, n_files] = lib_util.get_all_filenames('data/');

% delete all that end with "packets"
for i = numel(filenames) : -1 : 1
    
    end_of_filename = filenames(i).name;
    
    end_of_filename = end_of_filename(end-10:end);
    
    if strcmp(end_of_filename, 'packets.mat') == true
        filenames(i) = [];
    end
end

% file by file
for file_id=1:1:numel(filenames)
    
    clc;
    
    filename = filenames(file_id);
    
    A21_analyze_single_file(file_id, filename);
    
    pause(0.5);
end
