clear all;
%close all;
clc;

% parameters that must correspond to the C++ program
ext_sys_param.samp_rate = 100e6;                            % complex samples per second
ext_sys_param.n_channels = 2;                               % number of rx antennas
ext_sys_param.data_type_re_im = 'float';                    % real and imag per complex samples, each 4 byte float, to 8 byte total per complex sample

% parameters that must be known before starting the system
ext_sys_param.center_freq = 5210;
ext_sys_param.bandwidth_vec = [80e6];                       % possible signal bandwidths

% deletes all old data
A01_create_or_clear_folders();

% simulated external trigger
for run_id=1:1:10
    
    clc;
    fprintf('Run id %d\n', run_id);
    
    % TRIGGER: now measure the channel
    A02_single_run(ext_sys_param, run_id);
end

% shut down c++ program
lib_data_usrp.shut_down_record_program();

disp('Gracefully shutting down.');
