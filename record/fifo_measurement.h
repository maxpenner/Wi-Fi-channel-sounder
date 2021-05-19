/*

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CHANNELSOUNDER_FIFO_CH_MEASUREMENT_H
#define CHANNELSOUNDER_FIFO_CH_MEASUREMENT_H

#include <vector>
#include <atomic>

namespace channelsounder
{
/*!
 * Inits unit internally. Must be called first.
 *
 * num_channels_arg             in our case this is the number of rx antennas
 * num_bytes_per_item_arg       one item is one complex sample with real and imag, e.g. with data type "float" it is num_bytes_per_item_arg=8
 * samp_rate_arg                sampling rate for each rx channel in Samples/s
 * return                       1 on success and 0 on failure
*/
int init_fifo_ch_measurement(const size_t n_channels_arg, const size_t n_bytes_per_item_arg);

/*!
 * Resets unit internally. Must be called when a new file is supposed to be recorded.
 *
 * n_samples                    number of samples we record and save, all samples after this are ignored
 * return                       1 on success and 0 on failure
*/
int reset_fifo_ch_measurement(const unsigned int n_samples, const unsigned int file_id);

/*!
 * Saves current time. Can be called anytime
 *
 * offset_ms                    receive offset in microseconds
 *
*/
void current_time(const unsigned int offset_microseconds);

/*!
 * Feed buffered samples. Size of single samples is known after initialization.
 *
 * buffs01                      vector of pointer to samples of individual channels
 * n_new_samples                number of new samples in buffer, buffer is guaranteed to be large enough
*/
void feed_new_ch_measurement(const std::vector<std::vector<char>> &buffs01, const unsigned long long n_new_samples);

/*!
 * Must be started in additional thread, processes unused half of fifo.
 * Saves measurements in binary file in ../data.
 * Must process faster than it takes to fill one fifo half, otherwise measurements are dropped.
 *
 * burst_timer_elapsed          when set to true, the thread has to finish
*/
void send_save_ch_measurements(std::atomic<bool>& burst_timer_elapsed);

/*!
 * Shows some stats of the fifo.
*/
void show_debug_information_fifo();
}

#endif
