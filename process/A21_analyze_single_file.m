function A21_analyze_single_file(file_id, filename)

    % load .mat file
    load(fullfile(filename.folder, filename.name), 'meta_and_analyzers');

    figure();
    clf();
    
    % iterate over each analyzer
    for analyzer_id=1:1:numel(meta_and_analyzers.analyzers)

        % get analyzer
        analyzer = meta_and_analyzers.analyzers{analyzer_id};

%         % show some results in terminal
%         detectionSummary(analyzer);
        
        % get all packets
        results = getResults(analyzer);
        
        % packet by packet
        for packet_id=1:1:numel(results)
            
            macSummary(analyzer, packet_id);

            wifi_packet = results{packet_id};

            if isempty(wifi_packet) == true
                disp('No wifi packets in this analyzer.');
                continue;
            end
            
            % PacketNum
            % Format
            % PacketOffset
            % NumRxSamples
            % PacketPower
            % Preamble
            % LSIG
            % RLSIG
            % PHYConfig             1 x n_users 
            % Status
            % PacketContents
            % RxPacket              n_samples x n_tx_ant
            % MAC                   1 x n_users
            % HESIGA    
            % HESIGB                contains substructure 1 x n_users
            % HEPreamble            contains substructure 1 x n_RUs
            % HEData                contains substructure User with corrspondence to RU, contains substructure RU with corrspondence to users

            % plot only those that we decoded successfully
            if strcmp(wifi_packet.Status, 'Success') == true
                
                % we have one channel estimation per RU
                n_ru = numel(wifi_packet.HEPreamble.RU);

                % ru by ru
                for ru_id=1:1:n_ru
                    
                    subplot(n_ru, 1, ru_id);

                    ru_data = wifi_packet.HEPreamble.RU(ru_id);

                    % extract the channel estimation
                    chanEst = ru_data.ChanEst;

                    % now plot the channel estimation
                    plot_channel_estimate(file_id, chanEst);
                end
            end
        end
    end
end

function plot_channel_estimate(file_id, chanEst)

    [n_subc, n_sts, n_rx_ant] = size(chanEst);
    
    colorarray = ["y", "m", "c", "r", "g", "b", "w", "k"];
    
    cnt_color = 1;
    
    for a=1:1:n_sts
        for b=1:1:n_rx_ant
            
            color_index = mod(cnt_color, numel(colorarray));
            
            plot(10*log10(abs(chanEst(:,a,b)).^2), colorarray(color_index));
            %semilogy(abs(chanEst(:,a,b)));
            hold on
            
            cnt_color = cnt_color + 1;
        end
    end
    title(strcat("File id: ", num2str(file_id)));
    xlabel('Subcarrier');
    ylabel('Power dB');
    left_right_space = 50;
    axis([-left_right_space n_subc+left_right_space -50 10]);
    grid on
end

