function [n_packets_valid] = A09_MAC_filter(analyzers)

    % this is an array of all address combinations found
    MAC_address_pairs_unique_array = [];

    % iterate over each analyzer
    for analyzer_id=1:1:numel(analyzers)

        analyzer = analyzers{analyzer_id};
        
        results = getResults(analyzer);
        
        % iterate over each packet found
        for packet_id=1:1:numel(results)

            wifi_packet = results{packet_id};

            if isempty(wifi_packet) == true
                disp('No wifi packets in this analyzer. Should never happen.');
                continue;
            end           
            
         	MAC_address_pairs_unique = check_packet_vailidity(wifi_packet);
            
            % save MAC address combination
            if isempty(MAC_address_pairs_unique) == false
                MAC_address_pairs_unique_array = [MAC_address_pairs_unique_array; MAC_address_pairs_unique];
            end            
        end
    end
    
    n_packets_valid = numel(MAC_address_pairs_unique_array);
    
    display_results(MAC_address_pairs_unique_array);
end

function [MAC_address_pairs_unique] = check_packet_vailidity(wifi_packet)

    % content of structure wifi_packet

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

    % for now we accept only Wi-Fi 6 HE-SU
    format = wifi_packet.Format;
    if strcmp(format,'HE-SU') == false
        MAC_address_pairs_unique = [];
        return;
    end             

    % The field MAC is an array of structures, one per user.
    % Any user can have one or more MPDUs (A-MPDU).
    % Each MPDU has an address combination.
    % We don't count packet coming from the AP.

    % save all address combinations found
    MAC_address_pairs = [];        

    % iterate over users
    for MAC_of_user_id=1:1:numel(wifi_packet.MAC)

        MAC_unit = wifi_packet.MAC(MAC_of_user_id);

        % each MPDU can be an A-MPDU
        n_mpdu = numel(MAC_unit.MPDU);                

        % iterate over all MPDUs of this user
        for MPDU_id=1:1:n_mpdu

            ToDS = MAC_unit.MPDU(MPDU_id).Config.ToDS;
            FromDS = MAC_unit.MPDU(MPDU_id).Config.FromDS; 
            Addr1 = MAC_unit.MPDU(MPDU_id).Config.Address1;
            Addr2 = MAC_unit.MPDU(MPDU_id).Config.Address2;
            Addr3 = MAC_unit.MPDU(MPDU_id).Config.Address3;
            Addr4 = MAC_unit.MPDU(MPDU_id).Config.Address4;                

            % MAC filter, we filter out frames with this combination as these packets come from the AP
            MAC_filter.FromDS = 1;
            MAC_filter.ToDS = 0;

            % did this MPDU come from the AP? if so, packet is not valid
            if ToDS == MAC_filter.ToDS && FromDS == MAC_filter.FromDS
                MAC_address_pairs_unique = [];
                return;
            end

            % concat to one large char
            str = strcat(num2str(ToDS), '_', num2str(FromDS), '_', Addr1, '_', Addr2, '_', Addr3, '_', Addr4);

            % save locally
            MAC_address_pairs = [MAC_address_pairs; string(str)];
        end
    end
    
    % get unique combinations of addresses
    MAC_address_pairs_unique = unique(MAC_address_pairs);
    
    % we only accept HE-SU, so there should be only one unique address combination
    if numel(MAC_address_pairs_unique) ~= 1
        
        fprintf('A09: HE-SU packet contains more than one address combination, is %d.\n', numel(MAC_address_pairs_unique));
        
        MAC_address_pairs_unique = [];
        return;
    end
end

function [] = display_results(MAC_address_pairs_unique_array)

    % sort
    MAC_address_pairs_unique_array_sorted = sort(MAC_address_pairs_unique_array);
    
    % get uniques and their amount
    [~, ia, ic] = unique(MAC_address_pairs_unique_array_sorted);
    MAC_address_pairs_unique = MAC_address_pairs_unique_array_sorted(ia);
    amount = accumarray(ic,1);
    
    % create new variable so see the content
    ToDS_FromDS_Addr0_Addr1_Addr2_Addr3 = MAC_address_pairs_unique;
    
    % display
    T = table(ToDS_FromDS_Addr0_Addr1_Addr2_Addr3, amount);
    
    % plot table
    % source: https://de.mathworks.com/matlabcentral/answers/254690-how-can-i-display-a-matlab-table-in-a-figure
    figure(3)
    clf()
    
    % Get the table in string form.
    TString = evalc('disp(T)');
    % Use TeX Markup for bold formatting and underscores.
    TString = strrep(TString,'<strong>','\bf');
    TString = strrep(TString,'</strong>','\rm');
    TString = strrep(TString,'_','\_');
    % Get a fixed-width font.
    FixedWidth = get(0,'FixedWidthFontName');
    % Output the table using the annotation command.
    annotation(gcf,'Textbox','String',TString,'Interpreter','Tex','FontName',FixedWidth,'Units','Normalized','Position',[0 0 1 1]); 
end





