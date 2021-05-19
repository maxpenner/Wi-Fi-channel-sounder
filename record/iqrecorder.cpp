//
// Copyright 2011-2015 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include <uhd/convert.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <atomic>
#include <chrono>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <thread>

// ##########################
// ##########################
// ##########################
#include <boost/asio.hpp>

// important links:
//  USRP N3xx cmd line args:    https://files.ettus.com/manual/page_usrp_n3xx.html
//  simple c++ example:         https://kb.ettus.com/Getting_Started_with_UHD_and_C%2B%2B
//  c++ functions:              https://files.ettus.com/manual/classuhd_1_1usrp_1_1multi__usrp.html#a72b7947cb0c434b98e9915f91b8f8fe0

#include "ringbuffer_rx.h"
#include "fifo_measurement.h"

 // these are all UHD parameters that are not set in the cmd line args
#define CS_RX_FREQ  1000e6      // default value set a startup
#define CS_RX_GAIN  0           // default value set a startup
#define CS_RX_BW    200e6       // N320 can't change analogue bandwidth, see http://ettus.80997.x6.nabble.com/USRP-users-N320-set-rx-bw-does-not-change-the-actual-BW-td13422.html
#define CS_RX_ANT   "RX2"
// ##########
// ##########
// ##########

namespace po = boost::program_options;

namespace {
constexpr int64_t CLOCK_TIMEOUT = 1000; // 1000mS timeout for external clock locking
} // namespace

/***********************************************************************
 * Test result variables
 **********************************************************************/
unsigned long long num_overruns      = 0;
unsigned long long num_underruns     = 0;
unsigned long long num_rx_samps      = 0;
unsigned long long num_tx_samps      = 0;
unsigned long long num_dropped_samps = 0;
unsigned long long num_seq_errors    = 0;
unsigned long long num_seqrx_errors  = 0; // "D"s
unsigned long long num_late_commands = 0;
unsigned long long num_timeouts_rx   = 0;
unsigned long long num_timeouts_tx   = 0;

inline boost::posix_time::time_duration time_delta(
    const boost::posix_time::ptime& ref_time)
{
    return boost::posix_time::microsec_clock::local_time() - ref_time;
}

inline std::string time_delta_str(const boost::posix_time::ptime& ref_time)
{
    return boost::posix_time::to_simple_string(time_delta(ref_time));
}

#define NOW() (time_delta_str(start_time))


/***********************************************************************
 * Benchmark RX Rate
 **********************************************************************/
void benchmark_rx_rate(uhd::usrp::multi_usrp::sptr usrp,
    const std::string& rx_cpu,
    uhd::rx_streamer::sptr rx_stream,
    bool random_nsamps,
    const boost::posix_time::ptime& start_time,
    std::atomic<bool>& burst_timer_elapsed,
    bool elevate_priority,
    double rx_delay)
{
    if (elevate_priority) {
        uhd::set_thread_priority_safe();
        // ##########################
        // ##########################
        // ##########################
        std::cout << "Elevating RX thread priority." << std::endl;
        // ##########
        // ##########
        // ##########
    }

    // print pre-test summary
    std::cout << boost::format("[%s] Testing receive rate %f Msps on %u channels") % NOW() % (usrp->get_rx_rate() / 1e6) % rx_stream->get_num_channels() << std::endl;

    // setup variables and allocate buffer
    uhd::rx_metadata_t md;
    const size_t max_samps_per_packet = rx_stream->get_max_num_samps();

    // ##########################
    // ##########################
    // ##########################
    std::uint16_t port = 8888;          // must be the same in matlab
    boost::asio::io_service io_context;
    boost::asio::ip::udp::endpoint receiver(boost::asio::ip::udp::v4(), port);
    boost::asio::ip::udp::socket socket(io_context, receiver);

    unsigned int cnt_measurement = 0;

    while(true){

        cnt_measurement++;

        std::cout << "Entered RX thread. Now awaiting new message via UDP. Current measurement cnt: " << cnt_measurement << std::endl;

        // we have two predefined messages
        const unsigned int max_message_length = 64;                                     // maximum length of message, must be the same in matlab
        std::string predefined_message_new_meas("New_Measurement_");                    // message for new measurement (16 Byte)
        std::string predefined_message_end_exec("End_Measurement_programm_now1234");    // message to shut down program (32 Byte)

        // receive the message, blocking call
        char buffer[max_message_length+256];                                            // add 256 Byte as security
        boost::asio::ip::udp::endpoint sender;
        std::size_t bytes_transferred = socket.receive_from(boost::asio::buffer(buffer), sender);

        // convert entire buffer to one string and resize to maximum message size
        std::string message_from_matlab(buffer);
        message_from_matlab.resize(max_message_length);

        // these variales are extracted from matlab message
        unsigned int file_id;
        unsigned int n_samples;

        // message to start new measurement?
        if (message_from_matlab.compare(0, predefined_message_new_meas.size(), predefined_message_new_meas) == 0){
            std::string::size_type sz;

            file_id = std::stoi(message_from_matlab.substr(16,8),&sz);                          // extract file id (8 Byte)
            unsigned int center_freq_MHz = std::stoi(message_from_matlab.substr(25,4),&sz);     // extract center frequency in MHz (4 Byte)
            n_samples = std::stoi(message_from_matlab.substr(30,10),&sz);                       // extract number of samples (10 Byte)

            // gains (4 Byte per channel)
            std::vector<unsigned int> gains;
            for (int j=0; j<rx_stream->get_num_channels(); j++){
                unsigned int gain = std::stoi(message_from_matlab.substr(41 + j*4,4),&sz);
                gains.push_back(gain);
            }

            // set new usrp parameters
            // source: https://files.ettus.com/manual/classuhd_1_1usrp_1_1multi__usrp.html#a72b7947cb0c434b98e9915f91b8f8fe0
            double rx_freq(center_freq_MHz);
            rx_freq = rx_freq*1e6;
            for (size_t ch = 0; ch < usrp->get_rx_num_channels(); ch++){
                uhd::tune_request_t tune_request(rx_freq);
                usrp->set_rx_freq(tune_request, ch);

                double rx_gain(gains[ch]);
                usrp->set_rx_gain(rx_gain, ch);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // terminate execution?
        else if (message_from_matlab.compare(0, predefined_message_end_exec.size(), predefined_message_end_exec) == 0){
            burst_timer_elapsed = true;
            std::cout << "Stopping program execution!" << std::endl;
            break;
        }
        // should never happen
        else{
            std::cout << "Unknown message: " << message_from_matlab << std::endl;
        }

        // reset the ringbuffer, so that is writes to initial buffer again
        channelsounder::reset_ringbuffer_rx();

        // reset the fifo, tell it how many samples we want to collect
        channelsounder::reset_fifo_ch_measurement(n_samples, file_id);

        unsigned long long n_new_samples = 0;

        // init target pointer
        std::vector<char*> buffs = channelsounder::get_ringbuffer_rx_pointers(0);
        // ##########
        // ##########
        // ##########

        bool had_an_overflow = false;
        uhd::time_spec_t last_time;
        const double rate = usrp->get_rx_rate();

        // ##########################
        // ##########################
        // ##########################
        //uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        //cmd.stream_now = false;
        //cmd.time_spec = uhd::time_spec_t(usrp->get_time_now() + uhd::time_spec_t(rx_delay));;//usrp->get_time_now() + uhd::time_spec_t(rx_delay);
        //rx_stream->issue_stream_cmd(cmd);

        uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
        cmd.num_samps = n_samples + 1000000;    // we stream additional samples just in case the driver on the host drops samples
        cmd.stream_now = false;
        cmd.time_spec = uhd::time_spec_t(usrp->get_time_now() + uhd::time_spec_t(rx_delay));
        rx_stream->issue_stream_cmd(cmd);

        // save current time
        float rx_delay_microseconds_f = rx_delay*1.0e6;
        channelsounder::current_time((unsigned int) rx_delay_microseconds_f);

        unsigned int stop_streaming_on_error = false;
        // ##########
        // ##########
        // ##########

        const float burst_pkt_time =
            std::max<float>(0.100f, (2 * max_samps_per_packet / rate));
        float recv_timeout = burst_pkt_time + rx_delay;

        // ##########################
        // ##########################
        // ##########################
        // some delay, necessary to remove error message "Receiver error: ERROR_CODE_TIMEOUT, continuing..."
        recv_timeout = recv_timeout + 3.0f;
        // ##########
        // ##########
        // ##########

        bool stop_called = false;
        while (true) {
            // if (burst_timer_elapsed.load(boost::memory_order_relaxed) and not stop_called)
            // {
            if (burst_timer_elapsed and not stop_called) {
                rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                stop_called = true;
            }
            if (random_nsamps) {
                //cmd.num_samps = rand() % max_samps_per_packet;
                //rx_stream->issue_stream_cmd(cmd);
            }
            try {
                // ##########################
                // ##########################
                // ##########################
                // retuns n_new_samples-many samples for each receive channel
                n_new_samples = rx_stream->recv(buffs, max_samps_per_packet, md, recv_timeout);

                // uhd counts samples for each channel
                num_rx_samps += n_new_samples * rx_stream->get_num_channels();

                // refresh pointers for next call of rx_stream->recv()
                buffs = channelsounder::get_ringbuffer_rx_pointers(n_new_samples);
                // ##########
                // ##########
                // ##########

                recv_timeout = burst_pkt_time;
            } catch (uhd::io_error& e) {
                std::cerr << "[" << NOW() << "] Caught an IO exception. " << std::endl;
                std::cerr << e.what() << std::endl;
                return;
            }

            // handle the error codes
            switch (md.error_code) {
                case uhd::rx_metadata_t::ERROR_CODE_NONE:
                    if (had_an_overflow) {
                        had_an_overflow          = false;
                        const long dropped_samps = (md.time_spec - last_time).to_ticks(rate);
                        if (dropped_samps < 0) {
                            std::cerr << "[" << NOW()
                                      << "] Timestamp after overrun recovery "
                                         "ahead of error timestamp! Unable to calculate "
                                         "number of dropped samples."
                                         "(Delta: "
                                      << dropped_samps << " ticks)\n";
                        }
                        num_dropped_samps += std::max<long>(1, dropped_samps);
                    }
                    if ((burst_timer_elapsed or stop_called) and md.end_of_burst) {
                        return;
                    }
                    // ##########################
                    // ##########################
                    // ##########################
                    //rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                    //md.end_of_burst = true;
                    // ##########
                    // ##########
                    // ##########
                    break;

                // ERROR_CODE_OVERFLOW can indicate overflow or sequence error
                case uhd::rx_metadata_t::ERROR_CODE_OVERFLOW:
                    last_time       = md.time_spec;
                    had_an_overflow = true;
                    // check out_of_sequence flag to see if it was a sequence error or
                    // overflow
                    if (!md.out_of_sequence) {
                        num_overruns++;
                    } else {
                        num_seqrx_errors++;
                        std::cerr << "[" << NOW() << "] Detected Rx sequence error."
                                  << std::endl;
                    }
                    // ##########################
                    // ##########################
                    // ##########################
                    //rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                    stop_streaming_on_error = true;
                    // ##########
                    // ##########
                    // ##########
                    break;

                case uhd::rx_metadata_t::ERROR_CODE_LATE_COMMAND:
                    std::cerr << "[" << NOW() << "] Receiver error: " << md.strerror()
                              << ", restart streaming..." << std::endl;
                    num_late_commands++;
                    // ##########################
                    // ##########################
                    // ##########################
                    // Radio core will be in the idle state. Issue stream command to restart
                    // streaming.
                    //cmd.time_spec  = usrp->get_time_now() + uhd::time_spec_t(0.05);
                    //cmd.stream_now = (buffs.size() == 1);
                    //rx_stream->issue_stream_cmd(cmd);

                    //rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                    stop_streaming_on_error = true;
                    // ##########
                    // ##########
                    // ##########
                    break;

                case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
                    if (burst_timer_elapsed) {
                        return;
                    }
                    std::cerr << "[" << NOW() << "] Receiver error: " << md.strerror()
                              << ", continuing..." << std::endl;
                    num_timeouts_rx++;
                    // ##########################
                    // ##########################
                    // ##########################
                    //rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                    stop_streaming_on_error = true;
                    // ##########
                    // ##########
                    // ##########
                    break;

                    // Otherwise, it's an error
                default:
                    std::cerr << "[" << NOW() << "] Receiver error: " << md.strerror()
                              << std::endl;
                    std::cerr << "[" << NOW() << "] Unexpected error on recv, continuing..."
                              << std::endl;
                    // ##########################
                    // ##########################
                    // ##########################
                    //rx_stream->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
                    stop_streaming_on_error = true;
                    // ##########
                    // ##########
                    // ##########
                    break;
            }

            // ##########################
            // ##########################
            // ##########################
            if (md.end_of_burst == true || stop_streaming_on_error == true){
                break;
            }
            // ##########
            // ##########
            // ##########
        }
    }
}

/***********************************************************************
 * Main code + dispatcher
 **********************************************************************/
int UHD_SAFE_MAIN(int argc, char* argv[])
{
    // all tx references removed

    // variables to be set by po
    std::string args;
    std::string rx_subdev;
    double duration;
    double rx_rate;
    std::string rx_otw;
    std::string rx_cpu;
    std::string mode, ref, pps;
    std::string channel_list, rx_channel_list;
    bool random_nsamps = false;
    std::atomic<bool> burst_timer_elapsed(false);
    size_t overrun_threshold, underrun_threshold, drop_threshold, seq_threshold;
    double rx_delay;
    std::string priority;
    bool elevate_priority = false;

    // setup the program options
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "help message")
        ("args", po::value<std::string>(&args)->default_value(""), "single uhd device address args")
        ("duration", po::value<double>(&duration)->default_value(10.0), "duration for the test in seconds")
        ("rx_subdev", po::value<std::string>(&rx_subdev), "specify the device subdev for RX")
        //("tx_subdev", po::value<std::string>(&tx_subdev), "specify the device subdev for TX")
        ("rx_rate", po::value<double>(&rx_rate), "specify to perform a RX rate test (sps)")
        //("tx_rate", po::value<double>(&tx_rate), "specify to perform a TX rate test (sps)")
        ("rx_otw", po::value<std::string>(&rx_otw)->default_value("sc16"), "specify the over-the-wire sample mode for RX")
        //("tx_otw", po::value<std::string>(&tx_otw)->default_value("sc16"), "specify the over-the-wire sample mode for TX")
        ("rx_cpu", po::value<std::string>(&rx_cpu)->default_value("fc32"), "specify the host/cpu sample mode for RX")
        //("tx_cpu", po::value<std::string>(&tx_cpu)->default_value("fc32"), "specify the host/cpu sample mode for TX")
        ("ref", po::value<std::string>(&ref), "clock reference (internal, external, mimo, gpsdo)")
        ("pps", po::value<std::string>(&pps), "PPS source (internal, external, mimo, gpsdo)")
        ("mode", po::value<std::string>(&mode), "DEPRECATED - use \"ref\" and \"pps\" instead (none, mimo)")
        ("random", "Run with random values of samples in send() and recv() to stress-test the I/O.")
        ("channels", po::value<std::string>(&channel_list)->default_value("0"), "which channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
        ("rx_channels", po::value<std::string>(&rx_channel_list), "which RX channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
        //("tx_channels", po::value<std::string>(&tx_channel_list), "which TX channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
        ("overrun-threshold", po::value<size_t>(&overrun_threshold), "Number of overruns (O) which will declare the benchmark a failure.")
        ("underrun-threshold", po::value<size_t>(&underrun_threshold), "Number of underruns (U) which will declare the benchmark a failure.")
        ("drop-threshold", po::value<size_t>(&drop_threshold), "Number of dropped packets (D) which will declare the benchmark a failure.")
        ("seq-threshold", po::value<size_t>(&seq_threshold), "Number of dropped packets (D) which will declare the benchmark a failure.")
    // NOTE: TX delay defaults to 0.25 seconds to allow the buffer on the device to fill completely
        //("tx_delay", po::value<double>(&tx_delay)->default_value(0.25), "delay before starting TX in seconds")
        ("rx_delay", po::value<double>(&rx_delay)->default_value(0.05), "delay before starting RX in seconds")
        ("priority", po::value<std::string>(&priority)->default_value("high"), "thread priority (high, normal)")
    ;
    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // print the help message
    if (vm.count("help") or (vm.count("rx_rate") + vm.count("tx_rate")) == 0) {
        std::cout << boost::format("UHD Benchmark Rate %s") % desc << std::endl;
        std::cout << "    Specify --rx_rate for a receive-only test.\n"
                     "    Specify --tx_rate for a transmit-only test.\n"
                     "    Specify both options for a full-duplex test.\n"
                  << std::endl;
        return ~0;
    }

    if (priority == "high") {
        uhd::set_thread_priority_safe();
        elevate_priority = true;
    }

    if (vm.count("mode")) {
        if (vm.count("pps") or vm.count("ref")) {
            std::cout << "ERROR: The \"mode\" parameter cannot be used with the \"ref\" "
                         "and \"pps\" parameters.\n"
                      << std::endl;
            return -1;
        } else if (mode == "mimo") {
            ref = pps = "mimo";
            std::cout << "The use of the \"mode\" parameter is deprecated.  Please use "
                         "\"ref\" and \"pps\" parameters instead\n"
                      << std::endl;
        }
    }

    // create a usrp device
    std::cout << std::endl;
    uhd::device_addrs_t device_addrs = uhd::device::find(args, uhd::device::USRP);
    if (not device_addrs.empty() and device_addrs.at(0).get("type", "") == "usrp1") {
        std::cerr << "*** Warning! ***" << std::endl;
        std::cerr << "Benchmark results will be inaccurate on USRP1 due to insufficient "
                     "features.\n"
                  << std::endl;
    }
    boost::posix_time::ptime start_time(boost::posix_time::microsec_clock::local_time());
    std::cout << boost::format("[%s] Creating the usrp device with: %s...") % NOW() % args
              << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // always select the subdevice first, the channel mapping affects the other settings
    if (vm.count("rx_subdev")) {
        usrp->set_rx_subdev_spec(rx_subdev);
    }

    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;
    int num_mboards = usrp->get_num_mboards();

    boost::thread_group thread_group;

    if (vm.count("ref")) {
        if (ref == "mimo") {
            if (num_mboards != 2) {
                std::cerr
                    << "ERROR: ref = \"mimo\" implies 2 motherboards; your system has "
                    << num_mboards << " boards" << std::endl;
                return -1;
            }
            usrp->set_clock_source("mimo", 1);
        } else {
            usrp->set_clock_source(ref);
        }

        if (ref != "internal") {
            std::cout << "Now confirming lock on clock signals..." << std::endl;
            bool is_locked = false;
            auto end_time =
                boost::get_system_time() + boost::posix_time::milliseconds(CLOCK_TIMEOUT);
            for (int i = 0; i < num_mboards; i++) {
                if (ref == "mimo" and i == 0)
                    continue;
                while ((is_locked = usrp->get_mboard_sensor("ref_locked", i).to_bool())
                           == false
                       and boost::get_system_time() < end_time) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (is_locked == false) {
                    std::cerr << "ERROR: Unable to confirm clock signal locked on board:"
                              << i << std::endl;
                    return -1;
                }
            }
        }
    }

    if (vm.count("pps")) {
        if (pps == "mimo") {
            if (num_mboards != 2) {
                std::cerr
                    << "ERROR: ref = \"mimo\" implies 2 motherboards; your system has "
                    << num_mboards << " boards" << std::endl;
                return -1;
            }
            // make mboard 1 a slave over the MIMO Cable
            usrp->set_time_source("mimo", 1);
        } else {
            usrp->set_time_source(pps);
        }
    }

    // check that the device has sufficient RX and TX channels available
    std::vector<std::string> channel_strings;
    std::vector<size_t> rx_channel_nums;
    if (vm.count("rx_rate")) {
        if (!vm.count("rx_channels")) {
            rx_channel_list = channel_list;
        }

        boost::split(channel_strings, rx_channel_list, boost::is_any_of("\"',"));
        for (size_t ch = 0; ch < channel_strings.size(); ch++) {
            size_t chan = std::stoul(channel_strings[ch]);
            if (chan >= usrp->get_rx_num_channels()) {
                throw std::runtime_error("Invalid channel(s) specified.");
            } else {
                rx_channel_nums.push_back(std::stoul(channel_strings[ch]));
            }
        }
    }

    std::cout << boost::format("[%s] Setting device timestamp to 0...") % NOW()
              << std::endl;
    if (pps == "mimo" or ref == "mimo") {
        // only set the master's time, the slave's is automatically sync'd
        usrp->set_time_now(uhd::time_spec_t(0.0), 0);
        // ensure that the setter has completed
        usrp->get_time_now();
        // wait for the time to sync
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // ##########################
    // ##########################
    // ##########################
    //} else if (rx_channel_nums.size() > 1 or tx_channel_nums.size() > 1) {
    } else if (rx_channel_nums.size() > 1) {
        //usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
        std::cout << "Channelsounder UHD_SAFE_MAIN(): Setting time to 0 at next pps." << std::endl;
        usrp->set_time_next_pps(uhd::time_spec_t(0.0));
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));

        // second synchronization
        const uhd::time_spec_t last_pps_time = usrp->get_time_last_pps();
        std::cout << "Channelsounder UHD_SAFE_MAIN(): Waiting for next pps in loop, then setting time to 0 at next rising edge of pps signal." << std::endl;
        while (last_pps_time == usrp->get_time_last_pps()){
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // This command will be processed fairly soon after the last PPS edge:
        usrp->set_time_next_pps(uhd::time_spec_t(0.0));
        std::cout << "Channelsounder UHD_SAFE_MAIN(): Done synchronizing to pps. USRPs have common time." << std::endl;
    // ##########
    // ##########
    // ##########
    } else {
        usrp->set_time_now(0.0);
    }

    // ##########################
    // ##########################
    // ##########################
    // source: https://kb.ettus.com/Getting_Started_with_UHD_and_C%2B%2B
    double rx_freq(CS_RX_FREQ);
    double rx_gain(CS_RX_GAIN);
    double rx_bw(CS_RX_BW);
    std::string rx_ant(CS_RX_ANT);
    std::cout << "Channelsounder UHD_SAFE_MAIN(): rx_channel_nums.size(): " << rx_channel_nums.size() << std::endl;
    // ##########
    // ##########
    // ##########

    // spawn the receive test thread
    if (vm.count("rx_rate")) {
        usrp->set_rx_rate(rx_rate);

        // ##########################
        // ##########################
        // ##########################
        // source: https://files.ettus.com/manual/classuhd_1_1usrp_1_1multi__usrp.html#a72b7947cb0c434b98e9915f91b8f8fe0
        for (size_t ch = 0; ch < rx_channel_nums.size(); ch++){
            uhd::tune_request_t tune_request(rx_freq);
            usrp->set_rx_freq(tune_request, ch);
            usrp->set_rx_gain(rx_gain, ch);
            usrp->set_rx_bandwidth(rx_bw, ch);
            usrp->set_rx_antenna(rx_ant, ch);
        }
        // ##########
        // ##########
        // ##########

        // create a receive streamer
        uhd::stream_args_t stream_args(rx_cpu, rx_otw);
        stream_args.channels             = rx_channel_nums;
        uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

        // ##########################
        // ##########################
        // ##########################
        // initialize fifo
        channelsounder::init_fifo_ch_measurement(rx_stream->get_num_channels(), uhd::convert::get_bytes_per_item(rx_cpu));
        auto save_thread = thread_group.create_thread([=, &burst_timer_elapsed]() {channelsounder::send_save_ch_measurements(burst_timer_elapsed);});
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

        // initialize ring buffer rx
        channelsounder::init_ringbuffer_rx(rx_stream->get_num_channels(), uhd::convert::get_bytes_per_item(rx_cpu), rx_stream->get_max_num_samps());
        auto process_thread = thread_group.create_thread([=, &burst_timer_elapsed]() {channelsounder::process_ringbuffer_rx(burst_timer_elapsed);});
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        // ##########
        // ##########
        // ##########

        auto rx_thread = thread_group.create_thread([=, &burst_timer_elapsed]() {
            benchmark_rx_rate(usrp,
                rx_cpu,
                rx_stream,
                random_nsamps,
                start_time,
                burst_timer_elapsed,
                elevate_priority,
                rx_delay);
        });
        uhd::set_thread_name(rx_thread, "bmark_rx_stream");
    }

    thread_group.join_all();

    // ##########################
    // ##########################
    // ##########################
    channelsounder::show_debug_information_ringbuffer_rx();
    channelsounder::show_debug_information_fifo();
    // ##########
    // ##########
    // ##########

    std::cout << "[" << NOW() << "] Benchmark complete." << std::endl << std::endl;

    // print summary
    const std::string threshold_err(" ERROR: Exceeds threshold!");
    const bool overrun_threshold_err = vm.count("overrun-threshold")
                                       and num_overruns > overrun_threshold;
    const bool underrun_threshold_err = vm.count("underrun-threshold")
                                        and num_underruns > underrun_threshold;
    const bool drop_threshold_err = vm.count("drop-threshold")
                                    and num_seqrx_errors > drop_threshold;
    const bool seq_threshold_err = vm.count("seq-threshold")
                                   and num_seq_errors > seq_threshold;
    std::cout << std::endl
              << boost::format("Benchmark rate summary:\n"
                               "  Num received samples:     %u\n"
                               "  Num dropped samples:      %u\n"
                               "  Num overruns detected:    %u\n"
                               "  Num transmitted samples:  %u\n"
                               "  Num sequence errors (Tx): %u\n"
                               "  Num sequence errors (Rx): %u\n"
                               "  Num underruns detected:   %u\n"
                               "  Num late commands:        %u\n"
                               "  Num timeouts (Tx):        %u\n"
                               "  Num timeouts (Rx):        %u\n")
                     % num_rx_samps % num_dropped_samps % num_overruns % num_tx_samps
                     % num_seq_errors % num_seqrx_errors % num_underruns
                     % num_late_commands % num_timeouts_tx % num_timeouts_rx
              << std::endl;
    // finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

    if (overrun_threshold_err || underrun_threshold_err || drop_threshold_err
        || seq_threshold_err) {
        std::cout << "The following error thresholds were exceeded:\n";
        if (overrun_threshold_err) {
            std::cout << boost::format("  * Overruns (%d/%d)") % num_overruns
                             % overrun_threshold
                      << std::endl;
        }
        if (underrun_threshold_err) {
            std::cout << boost::format("  * Underruns (%d/%d)") % num_underruns
                             % underrun_threshold
                      << std::endl;
        }
        if (drop_threshold_err) {
            std::cout << boost::format("  * Dropped packets (RX) (%d/%d)")
                             % num_seqrx_errors % drop_threshold
                      << std::endl;
        }
        if (seq_threshold_err) {
            std::cout << boost::format("  * Dropped packets (TX) (%d/%d)")
                             % num_seq_errors % seq_threshold
                      << std::endl;
        }
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
