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
#include <variant>

class FileDevice : public SoapySDR::Device {
private:
  std::filesystem::path path;

  std::variant<std::ifstream, std::ofstream> *file = nullptr;

  int stream_type; // 0: CS8, 1: CF32

  SoapySDR::Stream *const RxStream = (SoapySDR::Stream *)1;
  SoapySDR::Stream *const TxStream = (SoapySDR::Stream *)2;

public:
  FileDevice(std::filesystem::path path) : path(path) {}

  SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                const std::vector<size_t> &channels = {},
                                const SoapySDR::Kwargs &args = {}) override {
    (void)channels;
    (void)args;

    SoapySDR::log(SOAPY_SDR_INFO,
                  std::format("setupStream: direction: {}, format: {}",
                              direction, format));

    if (file != nullptr) {
      SoapySDR::log(SOAPY_SDR_ERROR, "Stream has already been set up");
      return NULL;
    }

    if (direction == SOAPY_SDR_RX) {
      if (format == SOAPY_SDR_CS8) {
        stream_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        stream_type = 1;
      } else {
        SoapySDR::log(SOAPY_SDR_ERROR, "Unsupported format (RX)");
        return NULL;
      }

      SoapySDR::logf(SOAPY_SDR_INFO, "Opening File(Rx) with path: %s",
                     path.c_str());

      // open file as input
      file =
          new std::variant<std::ifstream, std::ofstream>(std::ifstream(path));

      return RxStream;
    } else {
      if (format != SOAPY_SDR_CS8) {
        SoapySDR::log(SOAPY_SDR_ERROR, "Unsupported format (TX)");
        return NULL;
      }

      SoapySDR::logf(SOAPY_SDR_INFO, "Opening File(Tx) with path: %s",
                     path.c_str());

      // open file as output
      file =
          new std::variant<std::ifstream, std::ofstream>(std::ofstream(path));

      return TxStream;
    }
  }

  size_t getStreamMTU(SoapySDR::Stream *stream) const override {
    (void)stream;

    // return 131072;
    return 0x20000;
  }

  int readStream(SoapySDR::Stream *stream, void *const *buffs,
                 const size_t numElems, int &flags, long long &timeNs,
                 const long timeoutUs = 100000) override {
    (void)stream;
    (void)flags;
    (void)timeNs;
    (void)timeoutUs;

    if (file == nullptr) {
      SoapySDR::log(SOAPY_SDR_ERROR, "Stream has not been set up");
      return 0;
    }

    if (file->index() == 1) {
      SoapySDR::log(SOAPY_SDR_ERROR, "File is opened as output");
      return 0;
    }

    std::ifstream &file = std::get<std::ifstream>(*this->file);

    void *buff = buffs[0];

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
    (void)stream;
    (void)buffs;
    (void)numElems;
    (void)flags;
    (void)timeNs;
    (void)timeoutUs;

    return 0;
  }
};

static SoapySDR::KwargsList find_file_device(const SoapySDR::Kwargs &args) {
  (void)args;

  SoapySDR::KwargsList devices = {{
      {"device", "File Device"},
      {"driver", "file"},
      {"label", "File Device"},
  }};

  for(auto &arg : args) {
    if (arg.first == "path") {
      devices[0]["path"] = arg.second;
    }
  }

  return devices;
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
