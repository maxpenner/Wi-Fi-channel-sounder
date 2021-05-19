function [packets] = get_wifi_packets(analyzers)

    % packets will be a cell array, each cell contains one packet
    packets = [];

    % analyzers is a cell array, each containing one handle to an analyzer
    for i=1:1:numel(analyzers)
        
        current_analyzer = analyzers{i};
        current_analyzer_packets = getResults(current_analyzer);
        
        % we want a row vector
        current_analyzer_packets = current_analyzer_packets';
        
        % concat
        packets = [packets; current_analyzer_packets];
    end
end