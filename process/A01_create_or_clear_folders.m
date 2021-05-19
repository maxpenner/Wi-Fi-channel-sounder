function [] = A01_create_or_clear_folders()

    % we save wifi packets in data
    if isfolder('../data') == false
        mkdir('../data')
    else
        % delete all old wifi 6 data
        lib_util.clear_directory('../data');
    end

    % folder for old records
    if isfolder('../data_old') == false
        mkdir('../data_old')
    end

    % we save wifi packets in data
    if isfolder('../data') == false
        mkdir('../data')
    else
        % delete all old wifi 6 data
        lib_util.clear_directory('../data');
    end

    % we save wifi packets in data
    if isfolder('data') == false
        mkdir('data')
    else
        % delete all old wifi 6 data
        lib_util.clear_directory('data');
    end
end
