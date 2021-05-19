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

#ifndef CHANNELSOUNDER_RINGBUFFER_RX_H
#define CHANNELSOUNDER_RINGBUFFER_RX_H

#include <vector>
#include <atomic>

namespace channelsounder
{
/*!
 * Inits unit internally. Must be called first.
 *
 * num_channels_arg             in our case this is the number of rx antennas
 * num_bytes_per_item_arg       one item is one complex sample with real and imag, e.g. with data type "float" it is num_bytes_per_item_arg=8
 * max_items_per_packet_arg     depends on what uhd driver does, tries to fully utilize 10Gbit/s bandwidth of ethernet NIC, needed for size of internal static memory
 * return                       1 on success and 0 on failure
*/
int init_ringbuffer_rx(const size_t n_channels_arg, const size_t n_bytes_per_item_arg, const size_t max_items_per_packet_arg);

/*!
 * Resets unit internally. This is the state is has after calling init_ringbuffer_rx(). Drops old samples in buffers.
 *
 * return                       1 on success and 0 on failure
*/
int reset_ringbuffer_rx();

/*!
 * Must be called initially with n_new_samples=0.
 * Breaks unit encapsulation, better solution needed.
 *
 * n_new_samples                number of new samples written per channel to pointers from last call
 * return                       vector of pointers pointing to internal static vectors (faster than dedicated write function), this is where uhd writes to
*/
std::vector<char*> get_ringbuffer_rx_pointers(const unsigned long long n_new_samples);

/*!
 * Must be started in additional thread, processes unused half of ringbuffer.
 * Must process faster than it takes to fill one buffer, otherwise samples are dropped.
 *
 * burst_timer_elapsed          when set to true, the thread has to finish
*/
void process_ringbuffer_rx(std::atomic<bool>& burst_timer_elapsed);

/*!
 * Shows some stats of the ring buffer.
*/
void show_debug_information_ringbuffer_rx();
}

#endif
