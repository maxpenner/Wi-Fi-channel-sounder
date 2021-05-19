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

#include <iostream>
#include <boost/thread/thread.hpp>

#include "debug.h"
#include "ringbuffer_rx.h"
#include "fifo_measurement.h"

#define N_COMPLEX_SAMPLES_PER_BUFFER        1000000

namespace channelsounder
{
static size_t n_channels;                   // number of channels/antennas, set in init function
static size_t n_bytes_per_item;             // size of complex sample
static size_t max_items_per_packet;         // maximum number of samples passed on by uhd driver

enum buffer_enum{
    NO_BUFFER = -1,
    BUFFER0 = 0,
    BUFFER1 = 1
};
static buffer_enum buffer2write;
static buffer_enum buffer2process;
static unsigned long long n_samples;        // number of samples written to current write buffer
static unsigned long long n_samples_old;    // number of samples written to last buffer used

// the static memory uhd will write to
// columns: number of rx channels (antennas)
// rows: container for samples
static std::vector<std::vector<char>> buffs0;
static std::vector<std::vector<char>> buffs1;

static boost::mutex m_mutex;
static boost::condition_variable m_condition;
    
// statistics
static unsigned long long n_buffer_full = 0;
static unsigned long long n_worker_not_done = 0;
static unsigned long long n_samples_total = 0;
static unsigned long long n_worker_wait = 0;
static unsigned long long n_worker_executed = 0;

int init_ringbuffer_rx(const size_t n_channels_arg, const size_t n_bytes_per_item_arg, const size_t max_items_per_packet_arg){
    n_channels = n_channels_arg;
    n_bytes_per_item = n_bytes_per_item_arg;
    max_items_per_packet = max_items_per_packet_arg;

    buffer2write = BUFFER0;
    buffer2process = NO_BUFFER;
    n_samples = 0;
    n_samples_old = 0;
    
    // initialize buffers
    std::vector<char> buff_template((N_COMPLEX_SAMPLES_PER_BUFFER + max_items_per_packet*2) * n_bytes_per_item);
    for (size_t ch = 0; ch < n_channels; ch++){
        // create one row for each channel/antenna
        buffs0.push_back(buff_template);
        buffs1.push_back(buff_template);
    }
    
    return 1;
}
    
int reset_ringbuffer_rx(){
    buffer2write = BUFFER0;
    buffer2process = NO_BUFFER;
    n_samples = 0;
    n_samples_old = 0;
    
    return 1;
}    
    
std::vector<char*> get_ringbuffer_rx_pointers(const unsigned long long n_new_samples){
    
    std::vector<char*> buffs_out;

    // current write buffer not full yet
    if(n_samples < N_COMPLEX_SAMPLES_PER_BUFFER){
        unsigned int offset = n_samples*n_bytes_per_item;
        if(buffer2write == BUFFER0){
            for (size_t ch = 0; ch < n_channels; ch++)
                buffs_out.push_back(&(buffs0[ch][offset]));
        }
        else{
            for (size_t ch = 0; ch < n_channels; ch++)
                buffs_out.push_back(&(buffs1[ch][offset]));           
        }
    }
    // buffer full, so switch buffer
    else{
        DBG_RB(n_buffer_full++;)
        n_samples_old = n_samples;
        n_samples = 0;
        {
            boost::mutex::scoped_lock lock(m_mutex, boost::try_to_lock);
            
            // if we were able to lock the mutex, the processing thread must be in waiting state, so we prepare processing and then notify worker thread
            if(lock){
                if(buffer2write == BUFFER0){
                    buffer2write = BUFFER1;
                    buffer2process = BUFFER0;
                    for (size_t ch = 0; ch < n_channels; ch++)
                        buffs_out.push_back(&(buffs1[ch][0]));
                }
                else{
                    buffer2write = BUFFER0;
                    buffer2process = BUFFER1;
                    for (size_t ch = 0; ch < n_channels; ch++)
                        buffs_out.push_back(&(buffs0[ch][0]));
                }
            }
            // if we were unable to lock the mutex, the processing thread is not done yet and we write data into the same buffer again, therefore losing samples
            else{
                DBG_RB(n_worker_not_done++;)
                buffer2process = NO_BUFFER;
                if(buffer2write == BUFFER0){
                    for (size_t ch = 0; ch < n_channels; ch++)
                        buffs_out.push_back(&(buffs0[ch][0]));
                }
                else{
                    for (size_t ch = 0; ch < n_channels; ch++)
                        buffs_out.push_back(&(buffs1[ch][0]));
                }            
            }
        }
        m_condition.notify_all();
    }
    
    DBG_RB(n_samples_total += n_new_samples;)
    n_samples += n_new_samples;    
    
    return buffs_out;
}
    
void process_ringbuffer_rx(std::atomic<bool>& burst_timer_elapsed){
    
    while(1){
            boost::mutex::scoped_lock lock(m_mutex);

            while(buffer2process == NO_BUFFER){
                DBG_RB(n_worker_wait++;)
                                    
                // from time to time we check if "burst_timer_elapsed" was set to true
                m_condition.wait_for(lock, boost::chrono::milliseconds(5000));
                
                // is set in main thread to stop execution
                if(burst_timer_elapsed == true)
                    return;
            }    

            DBG_RB(n_worker_executed++;)

            if(buffer2process == BUFFER0)
                feed_new_ch_measurement(buffs0, n_samples_old);
            else if(buffer2process == BUFFER1)
                feed_new_ch_measurement(buffs1, n_samples_old);

            // we are done, make sure we enter wait loop
            buffer2process = NO_BUFFER;
    }
}
    
void show_debug_information_ringbuffer_rx(){
    std::cout << "--------------------------" << std::endl;
    std::cout << "ringbuffer_rx" << std::endl;
    std::cout << "n_buffer_full: " << n_buffer_full << std::endl;
    std::cout << "n_worker_not_done: " << n_worker_not_done << std::endl;
    std::cout << "n_samples_total: " << n_samples_total << std::endl;
    std::cout << "n_worker_wait: " << n_worker_wait << std::endl;
    std::cout << "n_worker_executed: " << n_worker_executed << std::endl;
    std::cout << "--------------------------" << std::endl; 
}
}
