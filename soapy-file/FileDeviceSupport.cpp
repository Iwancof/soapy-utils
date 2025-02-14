#include <SoapySDR/Device.hpp>
#include <SoapySDR/Errors.h>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Version.h>

#include <SoapySDR/Formats.h>

#include <complex>
#include <iostream>

#include <string.h>

class FileDevice : public SoapySDR::Device {
private:
  char *path;

  int file_type;   // 0: HackRF transfer format, 1: Custom format
  int stream_type; // 0: CS8, 1: CF32

  void path_to_type() {
    // extension of `path`
    char *ext = strrchr(path, '.');
    if (ext == NULL) {
      throw std::runtime_error("Failed to get extension of the file");
    }

    if (strcmp(ext, ".dat") == 0) {
      file_type = 0;
    } else if (strcmp(ext, ".txt") == 0) {
      file_type = 1;
    } else {
      throw std::runtime_error("Unsupported file type");
    }
  }

public:
  FileDevice(char *path) : path(path) { path_to_type(); }

  SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                const std::vector<size_t> &channels = {},
                                const SoapySDR::Kwargs &args = {}) override {
    (void)channels;
    (void)args;

    // SoapySDR::log(SOAPY_SDR_INFO,
    //               std::format("setupStream: direction: {}, format: {}",
    //                           direction, format));

    SoapySDR::logf(SOAPY_SDR_INFO,
                   "setupStream: direction: %d, format: %s, file_type: %d",
                   direction, format.c_str(), file_type);

    if (direction == SOAPY_SDR_RX) {
      if (format == SOAPY_SDR_CS8) {
        stream_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        stream_type = 1;
      } else {
        throw std::runtime_error("Unsupported format (RX)");
      }

      SoapySDR::logf(SOAPY_SDR_INFO, "Opening File(Rx) with path: %s", path);

      FILE *fp = fopen(path, "r");

      if (fp == NULL) {
        throw std::runtime_error("Failed to open file");
      }

      return (SoapySDR::Stream *)fp;
    } else {
      if (format == SOAPY_SDR_CS8) {
        stream_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        stream_type = 1;
      } else {
        throw std::runtime_error("Unsupported format (TX)");
      }

      SoapySDR::logf(SOAPY_SDR_INFO, "Opening File(Tx) with path: %s", path);

      FILE *fp = fopen(path, "w");

      if (fp == NULL) {
        throw std::runtime_error("Failed to open file");
      }

      return (SoapySDR::Stream *)fp;
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

    FILE *fp = (FILE *)stream;

    void *buff = buffs[0];

    std::complex<int8_t> *buffer_ci8 = (std::complex<int8_t> *)buff;
    std::complex<float> *buffer_cf32 = (std::complex<float> *)buff;

    if (file_type == 0) {
      int num_read = 0;
      std::complex<int8_t> tmp;

      while (feof(fp) != EOF) {
        if (num_read >= numElems) {
          break;
        }

        if (fread(&tmp, sizeof(int8_t), 2, fp) != 2) {
          throw std::runtime_error("Failed to read data from file");
        }

        if (stream_type == 0) {
          buffer_ci8[num_read] = tmp;
        } else {
          buffer_cf32[num_read] = std::complex<float>(
              (float)tmp.real() / 127.0f, (float)tmp.imag() / 127.0f);
        }

        num_read += 1;
      }

      // std::cout << "num_read: " << num_read << std::endl;

      // get file offset
      // long offset = ftell(fp);
      // std::cout << "offset: " << offset << std::endl;

      return num_read;
    } else if (file_type == 1) {
      if (feof(fp)) {
        return SOAPY_SDR_UNDERFLOW;
      }
      size_t size_in_file;
      if (fscanf(fp, "%zu\n", &size_in_file) != 1) {
        throw std::runtime_error("Failed to read size from file");
      }

      SoapySDR::logf(SOAPY_SDR_TRACE,
                     "readStream: numElems: %zu, size_in_file: %zu", numElems,
                     size_in_file);

      if (numElems < size_in_file) {
        throw std::runtime_error("Buffer size is smaller than the file size");
      }

      for (size_t i = 0; i < size_in_file; i++) {
        int real, imag;

        if (fscanf(fp, "%d %d\n", &real, &imag) != 2) {
          throw std::runtime_error("Failed to read data from file");
        }

        if (stream_type == 0) {
          buffer_ci8[i] = std::complex<int8_t>(real, imag);
        } else {
          buffer_cf32[i] =
              std::complex<float>((float)real / 127.0f, (float)imag / 127.0f);
        }
      }

      return size_in_file;
    } else {
      throw std::runtime_error("Unsupported file type");
    }
  }

  int writeStream(SoapySDR::Stream *stream, const void *const *buffs,
                  const size_t numElems, int &flags, const long long timeNs = 0,
                  const long timeoutUs = 100000) override {
    if (file_type != 1) {
      throw std::runtime_error("Unsupported file type(Tx)");
    }

    FILE *fp = (FILE *)stream;

    const void *buff = buffs[0];

    const std::complex<int8_t> *buffer_ci8 = (const std::complex<int8_t> *)buff;
    const std::complex<float> *buffer_cf32 = (const std::complex<float> *)buff;

    fprintf(fp, "%zu\n", numElems);

    SoapySDR::logf(SOAPY_SDR_TRACE, "writeStream: numElems: %zu", numElems);

    for (size_t i = 0; i < numElems; i++) {
      if (stream_type == 0) {
        fprintf(fp, "%d %d ", (int)buffer_ci8[i].real(),
                (int)buffer_ci8[i].imag());
      } else {
        fprintf(fp, "%d %d ", (int)(buffer_cf32[i].real() * 127.0f),
                (int)(buffer_cf32[i].imag() * 127.0f));
      }
    }

    fprintf(fp, "\n");

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

  char *path;
  for (auto &arg : args) {
    if (arg.first == "path") {
      path = strdup(arg.second.c_str());
    }
  }

  return new FileDevice(path);
}

static SoapySDR::Registry register_file_device("file", &find_file_device,
                                               &make_file_device,
                                               SOAPY_SDR_ABI_VERSION);
