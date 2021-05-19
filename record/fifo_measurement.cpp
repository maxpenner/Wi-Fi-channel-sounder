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
#include <fstream>
#include <boost/thread/thread.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iomanip>

#include "debug.h"
#include "config.h"
#include "fifo_measurement.h"

static unsigned int CH_MEASUREMENT_LENGTH_IN_SAMPLES = 1000000;
static unsigned int FILE_ID = 0;

namespace channelsounder
{
static size_t n_channels;                       // number of channels/antennas, set in init function
static size_t n_bytes_per_item;                 // size of complex sample

enum buffer_enum{
    NO_BUFFER = -1,
    BUFFER0 = 0,
};
static buffer_enum buffer2process;

// collecting samples in a state machine
enum buffer_enum_state{
    COLLECT_CHANNEL_MEASUREMENT,
    DROP_SAMPLES
};
static buffer_enum_state d_STATE;
static unsigned int n_state;                    // state counter

// columns: number of rx channels (antennas)
// rows: container for samples
static std::vector<std::vector<char>> buffs0;

static boost::mutex m_mutex;
static boost::condition_variable m_condition;

static uint64_t current_time_since_epoch_microseconds;

// statistics
static unsigned long long n_measurement_saved = 0;
static unsigned long long n_samples_total = 0;
static unsigned long long n_worker_not_done = 0;
static unsigned long long n_worker_wait = 0;
static unsigned long long n_worker_executed = 0;

int init_fifo_ch_measurement(const size_t n_channels_arg, const size_t n_bytes_per_item_arg){
    n_channels = n_channels_arg;
    n_bytes_per_item = n_bytes_per_item_arg;

    buffer2process = NO_BUFFER;

    d_STATE = COLLECT_CHANNEL_MEASUREMENT;
    n_state = 0;

    buffs0.clear();

    // initialize buffers
    std::vector<char> buff_template(CH_MEASUREMENT_LENGTH_IN_SAMPLES * n_bytes_per_item);
    for (size_t ch = 0; ch < n_channels; ch++)
        buffs0.push_back(buff_template);

    return 1;
}

int reset_fifo_ch_measurement(const unsigned int n_samples, const unsigned int file_id){

    CH_MEASUREMENT_LENGTH_IN_SAMPLES = n_samples;
    FILE_ID = file_id;

    buffer2process = NO_BUFFER;

    d_STATE = COLLECT_CHANNEL_MEASUREMENT;
    n_state = 0;

    buffs0.clear();

    // initialize buffers
    std::vector<char> buff_template(CH_MEASUREMENT_LENGTH_IN_SAMPLES * n_bytes_per_item);
    for (size_t ch = 0; ch < n_channels; ch++)
        buffs0.push_back(buff_template);

    return 1;
}

void current_time(const unsigned int offset_microseconds){

    // measure time since epoch
    boost::posix_time::ptime const time_epoch(boost::gregorian::date(1970, 1, 1));
    auto ms = (boost::posix_time::microsec_clock::local_time() - time_epoch).total_microseconds();

    // convert to microseconds and and add offset
    current_time_since_epoch_microseconds = (uint64_t) ms;
    current_time_since_epoch_microseconds = current_time_since_epoch_microseconds + (uint64_t) offset_microseconds;
}

void feed_new_ch_measurement(const std::vector<std::vector<char>> &buffs01, const unsigned long long n_new_samples){
    DBG_RB(n_samples_total += n_new_samples;)
    unsigned int n_consumed_samples = 0;

    while(n_consumed_samples < n_new_samples)
    {
        switch(d_STATE)
        {
            case COLLECT_CHANNEL_MEASUREMENT:
            {
                unsigned int n_residual_samples = n_new_samples - n_consumed_samples;
                unsigned int n_samples_until_measurement_complete = CH_MEASUREMENT_LENGTH_IN_SAMPLES - n_state;
                unsigned int n_samples_usable = std::min(n_samples_until_measurement_complete, n_residual_samples);

                // save binary data of this measurement
                unsigned int offest = n_state * n_bytes_per_item;
                for(size_t ch = 0; ch < n_channels; ch++){
                    auto destination = buffs0[ch].begin() + offest;
                    auto source = buffs01[ch].begin() + n_consumed_samples*n_bytes_per_item;
                    std::copy_n(source, n_samples_usable*n_bytes_per_item, destination);
                }

                n_state += n_samples_usable;
                n_consumed_samples += n_samples_usable;

                // if this condition is met, we know that the measurement is complete
                if(n_state == CH_MEASUREMENT_LENGTH_IN_SAMPLES){
                    d_STATE = DROP_SAMPLES;
                    n_state = 0;

                    // trigger worker thread
                    {
                        boost::mutex::scoped_lock lock(m_mutex, boost::try_to_lock);

                        // if we were able to lock the mutex, processing thread must be in waiting state, so we prepare processing and then notify worker thread
                        if(lock){
                            buffer2process = BUFFER0;
                        }
                        // if we were unable to lock the mutex, we write data into the same buffer again, therefore losing samples
                        else{
                            DBG_RB(n_worker_not_done++;)
                            buffer2process = NO_BUFFER;
                        }
                    }
                    m_condition.notify_all();
                }
            }
            break;

            case DROP_SAMPLES:
            {
                unsigned int n_residual_samples = n_new_samples - n_consumed_samples;

                n_consumed_samples += n_residual_samples;
            }
            break;
        }
    }
}

void send_save_ch_measurements(std::atomic<bool>& burst_timer_elapsed){

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

            // create, save and close file
            std::ostringstream ss;
            ss << std::setw(10) << std::setfill('0') << n_measurement_saved << "_";
            ss << std::setw(10) << std::setfill('0') << FILE_ID << "_";
            ss << std::setw(20) << std::setfill('0') << current_time_since_epoch_microseconds;

            std::string str_n_measurement_saved = ss.str();
            std::string folder_path = SAVE_PATH;
            std::string file_name = "iqrecord_";
            std::string full_file_path = folder_path + file_name + str_n_measurement_saved + ".bin";
            n_measurement_saved++;
            std::ofstream fout(full_file_path, std::ios::out | std::ios::binary);

            // write data from buffer
            for(size_t ch = 0; ch < n_channels; ch++)
                fout.write((char*)&buffs0[ch][0], buffs0[ch].size()*sizeof(buffs0[ch][0]));

            fout.close();

            // we are done, make sure we enter wait loop
            buffer2process = NO_BUFFER;
    }
}

void show_debug_information_fifo(){
    std::cout << "--------------------------" << std::endl;
    std::cout << "fifo_measurement" << std::endl;
    std::cout << "n_measurement_saved: " << n_measurement_saved << std::endl;
    std::cout << "n_samples_total: " << n_samples_total << std::endl;
    std::cout << "n_worker_not_done: " << n_worker_not_done << std::endl;
    std::cout << "n_worker_wait: " << n_worker_wait << std::endl;
    std::cout << "n_worker_executed: " << n_worker_executed << std::endl;
    std::cout << "--------------------------" << std::endl;
}
}
