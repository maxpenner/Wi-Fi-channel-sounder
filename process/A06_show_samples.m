function [] = A06_show_samples(complex_samples_all_antennas, samp_rate)

    [n_samples, n_rx_ant] = size(complex_samples_all_antennas);
    
    n_samples = min(n_samples, 5e6);
    complex_samples_all_antennas = complex_samples_all_antennas(1:n_samples, :);    
    
    figure(2)
    clf()
    for ch_rx=1:1:n_rx_ant
        
        subplot(n_rx_ant,1,ch_rx);
        
        plot(real(complex_samples_all_antennas(:, ch_rx)));
        hold on
        plot(imag(complex_samples_all_antennas(:, ch_rx)),'r');
        
        title("RX Samples of Channel Index " + num2str(ch_rx-1) + " @ " + num2str(samp_rate/1e6) +  "MS/s");
        legend('real','imag');
        xlabel("Sample Index");
        ylabel("Amplitude");
        axis([-n_samples*0.1 n_samples*1.1 -1.5 1.5]);
        grid on
    end
end

