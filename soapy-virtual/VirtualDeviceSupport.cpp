#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Version.h>

#include <complex>
#include <format>
#include <iostream>
#include <mutex>
#include <queue>

std::queue<std::complex<int8_t>> world;
class VirtualDevice : public SoapySDR::Device {

private:
  std::queue<std::complex<int8_t>> world;
  std::mutex world_mutex;

  class VirtualStream {
  private:
    bool in_burst;

  public:
    int interface_type; // 0: CS8, 1: CF32

    VirtualStream(const std::string &format) {
      if (format == SOAPY_SDR_CS8) {
        interface_type = 0;
      } else if (format == SOAPY_SDR_CF32) {
        interface_type = 1;
      } else {
        throw std::runtime_error("Unsupported format");
      }

      in_burst = false;
    }

    void start_burst() { in_burst = true; }
    void end_burst() { in_burst = false; }
    bool is_burst() { return in_burst; }

    size_t append_type(int type, std::queue<std::complex<int8_t>> &world,
                       const void *const buff, const size_t numElems) {
      if (type == 0) {
        const std::complex<int8_t> *buffer = (const std::complex<int8_t> *)buff;
        for (size_t i = 0; i < numElems; i++) {
          world.push(buffer[i]);
        }
      } else {
        const std::complex<float> *buffer = (const std::complex<float> *)buff;
        for (size_t i = 0; i < numElems; i++) {
          world.push(std::complex<int8_t>(buffer[i].real() * 127.0f,
                                          buffer[i].imag() * 127.0f));
        }
      }

      return numElems;
    }

    size_t read(std::queue<std::complex<int8_t>> &world, const void *const buff,
                const size_t numElems) {
      size_t count = 0;

      while (count < numElems && !world.empty()) {
        if (interface_type == 0) {
          std::complex<int8_t> *buffer = (std::complex<int8_t> *)buff;
          buffer[count] = world.front();
        } else {
          std::complex<float> *buffer = (std::complex<float> *)buff;
          buffer[count] = std::complex<float>(world.front().real() / 127.0f,
                                              world.front().imag() / 127.0f);
        }

        world.pop();
        count++;
      }

      return count;
    }
  };

public:
  VirtualDevice() {
    SoapySDR_logf(SOAPY_SDR_INFO, "Opening Virtual Device: %p", &world);
  }

  size_t getStreamMTU(SoapySDR::Stream *stream) const override {
    (void)stream;
    return 16 * 20;
  }

  SoapySDR::Stream *setupStream(const int direction, const std::string &format,
                                const std::vector<size_t> &channels = {},
                                const SoapySDR::Kwargs &args = {}) override {
    (void)channels;
    (void)args;

    SoapySDR_logf(SOAPY_SDR_INFO, "setupStream: direction: %d, format: %s",
                  direction, format.c_str());

    return (SoapySDR::Stream *)new VirtualStream(format);
  }

  int readStream(SoapySDR::Stream *stream, void *const *buffs,
                 const size_t numElems, int &flags, long long &timeNs,
                 const long timeoutUs = 100000) override {
    std::lock_guard<std::mutex> lock(world_mutex);
    VirtualStream *vstream = (VirtualStream *)stream;

    return vstream->read(world, buffs[0], numElems);
  }

  int writeStream(SoapySDR::Stream *stream, const void *const *buffs,
                  const size_t numElems, int &flags, const long long timeNs = 0,
                  const long timeoutUs = 100000) override {
    std::lock_guard<std::mutex> lock(world_mutex);

    VirtualStream *vstream = (VirtualStream *)stream;
    if (!vstream->is_burst()) {
      int phase_idx = 0;

      // append not data signal
      std::complex<float> predata[500];
      for (int i = 0; i < 500; i++) {
        // no_data[i] = 1e-03 * std::exp(I * 2 * M_PI * 0.0193f * phase_idx);
        predata[i] =
            1e-2f *
            std::exp(std::complex<float>(0, 2 * M_PI * 0.0193f * phase_idx));
        phase_idx++;
      }
      vstream->append_type(1, world, predata, 500);

      // append predata signal
      for (int i = 0; i < 50; i++) {
        predata[i] =
            (1e-2f +
             (1e-1f - 1e-2f) *
                 (0.5f - 0.5f * cosf(M_PI * (float)(phase_idx) / 50.0f))) *
            std::exp(std::complex<float>(0, 2 * M_PI * 0.0193f * phase_idx));
        phase_idx++;
      }
      vstream->append_type(1, world, predata, 50);

      vstream->start_burst();
    }

    // append data signal
    size_t num_wrote = vstream->append_type(vstream->interface_type, world,
                                            buffs[0], numElems);

    if (flags & SOAPY_SDR_END_BURST) {
      int phase_idx = 0;
      // TODO: append postdata signal

      // append not data signal
      std::complex<float> postdata[500];
      for (int i = 0; i < 500; i++) {
        // no_data[i] = 1e-03 * std::exp(I * 2 * M_PI * 0.0193f * phase_idx);
        postdata[i] =
            1e-2f *
            std::exp(std::complex<float>(0, 2 * M_PI * 0.0193f * phase_idx));
        phase_idx++;
      }
      vstream->append_type(1, world, postdata, 500);

      vstream->end_burst();
    }

    return num_wrote;
  }
};

static SoapySDR::KwargsList find_virtual_device(const SoapySDR::Kwargs &args) {
  (void)args;

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
