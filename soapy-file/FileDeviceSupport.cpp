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
      throw std::runtime_error("Stream has already been set up");
    }

    if (direction == SOAPY_SDR_RX) {
      if (format == SOAPY_SDR_CS8) {
        stream_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        stream_type = 1;
      } else {
        throw std::runtime_error("Unsupported format (RX)");
      }

      SoapySDR::logf(SOAPY_SDR_INFO, "Opening File(Rx) with path: %s",
                     path.c_str());

      // open file as input
      file =
          new std::variant<std::ifstream, std::ofstream>(std::ifstream(path));

      return RxStream;
    } else {
      if (format == SOAPY_SDR_CS8) {
        stream_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        stream_type = 1;
      } else {
        throw std::runtime_error("Unsupported format (TX)");
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

    if (stream != RxStream) {
      throw std::runtime_error("Stream is not rx stream");
    }

    if (file == nullptr) {
      throw std::runtime_error("Stream has not been set up");
    }

    if (file->index() == 1) {
      throw std::runtime_error("File is opened as output");
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

    if (stream != TxStream) {
      throw std::runtime_error("Stream is not tx stream");
    }

    if (file == nullptr) {
      throw std::runtime_error("Stream has not been set up");
    }

    if (file->index() == 0) {
      throw std::runtime_error("File is opened as input");
    }

    std::ofstream &file = std::get<std::ofstream>(*this->file);

    const void *buff = buffs[0];

    const std::complex<int8_t> *buffer_ci8 = (const std::complex<int8_t> *)buff;
    const std::complex<float> *buffer_cf32 = (const std::complex<float> *)buff;

    file << numElems << std::endl;

    for (size_t i = 0; i < numElems; i++) {
      if (stream_type == 0) {
        file << (int)buffer_ci8[i].real() << " " << (int)buffer_ci8[i].imag()
             << std::endl;
      } else {
        file << (int)(buffer_cf32[i].real() * 127.0f) << " "
             << (int)(buffer_cf32[i].imag() * 127.0f) << std::endl;
      }
    }

    return numElems;
  }
};

static SoapySDR::KwargsList find_file_device(const SoapySDR::Kwargs &args) {
  (void)args;

  SoapySDR::KwargsList devices = {{
      {"device", "File Device"},
      {"driver", "file"},
      {"label", "File Device"},
  }};

  for (auto &arg : args) {
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
