#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Version.h>
#include <SoapySDR/Logger.hpp>

#include <iostream>
#include <format>
#include <queue>
#include <complex>

std::queue<std::complex<int8_t>> world;

class VirtualDevice : public SoapySDR::Device {
public:
  int readStream(SoapySDR::Stream *stream, void *const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs = 100000) override {
    return 0;
    // std::string log = std::format("readStream: stream: {}, buffs: {}, numElems: {}, flags: {}, timeNs: {}, timeoutUs: {}", (void*)stream, (void*)buffs, numElems, flags, timeNs, timeoutUs);
    // SoapySDR::log(SOAPY_SDR_INFO, log);

    // // Read from the world
    // std::complex<int8_t> *buffer = (std::complex<int8_t> *)buffs[0]; // channel 0
    // size_t i;
    // for (i = 0; i < numElems; i++) {
    //   if (world.empty()) {
    //     break;
    //   }

    //   buffer[i] = world.front();
    //   world.pop();
    // }

    // return i;
  }

  int writeStream(SoapySDR::Stream *stream, const void *const *buffs, const size_t numElems, int &flags, const long long timeNs = 0, const long timeoutUs = 100000) override {
    return 0;
    // std::string log = std::format("writeStream: stream: {}, buffs: {}, numElems: {}, flags: {}, timeNs: {}, timeoutUs: {}", (void*)stream, (void*)buffs, numElems, flags, timeNs, timeoutUs);
    // SoapySDR::log(SOAPY_SDR_INFO, log);

    // // Write to the world
    // const std::complex<int8_t> *buffer = (const std::complex<int8_t> *)buffs[0]; // channel 0
    // for (size_t i = 0; i < numElems; i++) {
    //   world.push(buffer[i]);
    // }

    // return numElems;
  }
};

static SoapySDR::KwargsList find_virtual_device(const SoapySDR::Kwargs &args) {
  std::cout << "find_virtual_device" << std::endl;
  for(auto &arg : args) {
    std::cout << arg.first << ": " << arg.second << std::endl;
  }
  std::cout << std::endl;

  return {{
      {"device", "Virtual Device"},
      {"driver", "virtual"},
      {"label", "Virtual Device"},
  }};
}

static SoapySDR::Device *make_virtual_device(const SoapySDR::Kwargs &args) {
  (void)args;

  return new VirtualDevice();
}

static SoapySDR::Registry register_virtual_device("virtual",
                                                  &find_virtual_device,
                                                  &make_virtual_device,
                                                  SOAPY_SDR_ABI_VERSION);
