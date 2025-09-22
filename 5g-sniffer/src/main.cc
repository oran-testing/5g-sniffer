/*
 * Copyright (c) 2021.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 */

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unistd.h>

#include "config.h"
#include "exceptions.h"
#include "file_sink.h"
#include "sniffer.h"
#include "spdlog/cfg/env.h"
#include "spdlog/spdlog.h"
#include "influx_client.h"
#include "globals.h"

using namespace std;
extern struct config config;

std::shared_ptr<DCIInfluxClient> influx_client_;

static void usage() {
  cout << "Usage: 5g_sniffer <path_to_config.toml>" << endl;
}

/**
 * Main function of the 5G sniffer.
 *
 * @param argc
 * @param argv
 */
int main(int argc, char **argv) {
  string config_path;

#ifdef DEBUG_BUILD
  SPDLOG_INFO("=== This is a debug mode build ===");
#endif

  // Load spdlog level from environment variable
  // For example: export SPDLOG_LEVEL=debug
  spdlog::cfg::load_env_levels();

  // Set logger pattern
  spdlog::set_pattern("[%^%l%$] [%H:%M:%S.%f thread %t] [%s:%#] %v");

  // Get config path from command-line args
  std::string zmq_address_override; // To hold the optional address

  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path_to_config.toml> [--zmq-addr <address>]" << std::endl;
    return 1;
  }
  
  // This loop finds the config path and the optional --zmq-addr flag
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--zmq-addr") {
      if (i + 1 < argc) {
        zmq_address_override = argv[++i];
      } else {
        std::cerr << "Error: --zmq-addr requires an address argument." << std::endl;
        return 1;
      }
    } else {
        config_path = arg; // Assume anything else is the config path
    }
  }

  if (config_path.empty()) {
      std::cerr << "Error: Config file path is required." << std::endl;
      return 1;
  }
  try {
    // Load the config
    config = config::load(config_path);

    std::string final_zmq_address;
    if (!zmq_address_override.empty()) {
        // Use the command-line override if it was provided
        final_zmq_address = zmq_address_override;
        std::cout << "Using command-line ZMQ address override." << std::endl;
    } else {
        final_zmq_address = "tcp://*:" + std::to_string(config.zmq_port);
    }
    std::cout << "ZMQ Publisher will listen on: " << final_zmq_address << std::endl;

    std::cout << "DEBUG: in MAIN before creation: " << influx_client_.get() << std::endl;
    if (config.influxdb_enabled) {
      std::cout << "--- InfluxDB is ENABLED. Creating Client... ---" << std::endl;
      influx_client_ = std::make_shared<DCIInfluxClient>(
        config.influxdb_url,
        config.influxdb_port,
        config.influxdb_org,
        config.influxdb_token,
        config.influxdb_bucket,
        config.identifier
      );
    std::cout << "DEBUG: in MAIN after creation: " << influx_client_.get() << std::endl;
    } else {
      std::cout << "--- InfluxDB is DISABLED. Skipping client creating. ---" << std::endl;
    }

    // Create sniffer
    if (config.file_path.compare("") == 0) {
      sniffer sniffer(config.sample_rate, config.frequency, config.rf_args,
                      config.ssb_numerology, final_zmq_address);
      sniffer.start();
    } else {
      sniffer sniffer(config.sample_rate, config.file_path.data(),
                      config.ssb_numerology, final_zmq_address);
      sniffer.start();
    }
  } catch (sniffer_exception &e) {
    SPDLOG_ERROR(e.what());
    return 1;
  } catch (const toml::parse_error &err) {
    std::cerr << "Error parsing sniffer configuration file '"
              << *err.source().path << "':\n"
              << err.description() << "\n  (" << err.source().begin << ")\n";
    return 1;
  }

  return 0;
}
