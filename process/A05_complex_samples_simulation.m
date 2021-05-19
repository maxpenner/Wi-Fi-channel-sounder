function [complex_samples, samp_rate_complex_samples, gain_used] = A05_complex_samples_simulation(n_channels, n_samples, bandwidth_vec)

    complex_samples = [];
    samp_rate_complex_samples = [];
    gain_used = [];

    fprintf('A05_complex_samples_simulation: Use one of the many Matlab examples to create complex samples.\n');
    fprintf('n_channels: %d\n', n_channels);
    fprintf('n_samples: %d\n', n_samples);
    for i=1:numel(bandwidth_vec)
        fprintf('bandwidth %d: %d\n', i, bandwidth_vec(i));
    end
end

