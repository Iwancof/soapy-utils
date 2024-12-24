#include <SoapySDR/Device.hpp>
#include <SoapySDR/Errors.h>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Version.h>

#include <SoapySDR/Formats.h>

#include <complex>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <queue>

class FileDevice : public SoapySDR::Device {
private:
  std::ifstream file;

  int stream_type; // 0: CS8, 1: CF32

public:
  FileDevice(std::filesystem::path path) : file(path) {
    // show path
    SoapySDR::logf(SOAPY_SDR_INFO, "Opening File Device with path: %s",
                   path.c_str());
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open file %s", path.c_str());
    }
  }

  SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                const std::vector<size_t> &channels = {},
                                const SoapySDR::Kwargs &args = {}) override {
    SoapySDR::log(SOAPY_SDR_INFO,
                  std::format("setupStream: direction: {}, format: {}",
                              direction, format));

    if (direction != SOAPY_SDR_RX) {
      SoapySDR::log(SOAPY_SDR_ERROR, "Only RX is supported");
      return NULL;
    }

    if (format == SOAPY_SDR_CS8) {
      stream_type = 0;
    } else if (format == SOAPY_SDR_CF32) {
      stream_type = 1;
    } else {
      SoapySDR::log(SOAPY_SDR_ERROR, "Unsupported format");
      return NULL;
    }

    return (SoapySDR::Stream *)1;
  }

  size_t getStreamMTU(SoapySDR::Stream *stream) const override {
    // return 131072;
    return 0x20000;
  }

  int readStream(SoapySDR::Stream *stream, void *const *buffs,
                 const size_t numElems, int &flags, long long &timeNs,
                 const long timeoutUs = 100000) override {
    void *buff = buffs[0];
    /*
    std::complex<int8_t> *buffer =
        (std::complex<int8_t> *)buffs[0]; // channel 0
    */
    std::complex<int8_t> *buffer_ci8 = (std::complex<int8_t> *)buff;
    std::complex<float> *buffer_cf32 = (std::complex<float> *)buff;

    if (file.eof()) {
      SoapySDR::log(SOAPY_SDR_ERROR, "file buffer has been EOF");
      return SOAPY_SDR_UNDERFLOW;
    }

    size_t size_in_file;
    file >> size_in_file;

    if (numElems < size_in_file) {
      SoapySDR::log(SOAPY_SDR_ERROR,
                    "Buffer size is smaller than the file size");
      return 0;
    }

    for (size_t i = 0; i < size_in_file; i++) {
      int real, imag;
      file >> real >> imag;
      if (stream_type == 0) {
        buffer_ci8[i] = std::complex<int8_t>(real, imag);
      } else {
        buffer_cf32[i] =
            std::complex<float>((float)real / 127.0f, (float)imag / 127.0f);
      }
    }

    return size_in_file;
  }

  int writeStream(SoapySDR::Stream *stream, const void *const *buffs,
                  const size_t numElems, int &flags, const long long timeNs = 0,
                  const long timeoutUs = 100000) override {
    return 0;
    // std::string log = std::format("writeStream: stream: {}, buffs: {},
    // numElems: {}, flags: {}, timeNs: {}, timeoutUs: {}", (void*)stream,
    // (void*)buffs, numElems, flags, timeNs, timeoutUs);
    // SoapySDR::log(SOAPY_SDR_INFO, log);

    // // Write to the world
    // const std::complex<int8_t> *buffer = (const std::complex<int8_t>
    // *)buffs[0]; // channel 0 for (size_t i = 0; i < numElems; i++) {
    //   world.push(buffer[i]);
    // }

    // return numElems;
  }
};

static SoapySDR::KwargsList find_file_device(const SoapySDR::Kwargs &args) {
  // std::cout << "find_file_device" << std::endl;
  // for (auto &arg : args) {
  //   std::cout << arg.first << ": " << arg.second << std::endl;
  // }
  // std::cout << std::endl;

  return {{
      {"device", "FIle Device"},
      {"driver", "file"},
      {"label", "File Device"},
  }};
}

static SoapySDR::Device *make_file_device(const SoapySDR::Kwargs &args) {
  // get `path` argument
  std::filesystem::path path;
  for (auto &arg : args) {
    if (arg.first == "path") {
      path = arg.second;
    }
  }

  return new FileDevice(path);
}

static SoapySDR::Registry register_file_device("file", &find_file_device,
                                               &make_file_device,
                                               SOAPY_SDR_ABI_VERSION);
