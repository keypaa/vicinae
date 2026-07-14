#include <algorithm>
#include <memory>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include "win32-audio-control.hpp"

namespace {

// Manual IPolicyConfig declaration (undocumented interface)
MIDL_INTERFACE("CA286FC3-91FD-42C3-8E9B-CAAFA66242E3")
IPolicyConfig : public IUnknown {
public:
  virtual HRESULT STDMETHODCALLTYPE GetMixFormat(void *, void **) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(void *, int, void **) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(void *, void *, void *) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(void *, void *) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetSharedModeFormat(void *, void **, void **, void **) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetSharedModeFormat(void *, void *, void *, void *, void *) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetCurrentShareMode(void *, void **) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(void *dev, DWORD role) = 0;
  virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(void *, int) = 0;
};

// {CA286FC3-91FD-42C3-8E9B-CAAFA66242E3}
static const IID IID_IPolicyConfig = {
    0xCA286FC3, 0x91FD, 0x42C3, {0x8E, 0x9B, 0xCA, 0xFA, 0x66, 0x24, 0x2E, 0x33}};

struct ComDeleter {
  void operator()(IUnknown *p) const {
    if (p) p->Release();
  }
};

template <typename T> using ComPtr = std::unique_ptr<T, ComDeleter>;

ComPtr<IMMDeviceEnumerator> createEnumerator() {
  IMMDeviceEnumerator *raw = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&raw));
  return ComPtr<IMMDeviceEnumerator>(SUCCEEDED(hr) ? raw : nullptr);
}

ComPtr<IMMDevice> defaultDevice(IMMDeviceEnumerator *enumerator, EDataFlow flow = eRender) {
  IMMDevice *raw = nullptr;
  HRESULT hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &raw);
  return ComPtr<IMMDevice>(SUCCEEDED(hr) ? raw : nullptr);
}

ComPtr<IAudioEndpointVolume> endpointVolume(IMMDevice *device) {
  IAudioEndpointVolume *raw = nullptr;
  HRESULT hr =
      device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&raw));
  return ComPtr<IAudioEndpointVolume>(SUCCEEDED(hr) ? raw : nullptr);
}

std::optional<QString> devicePropertyString(IMMDevice *device, const PROPERTYKEY &key) {
  IPropertyStore *props = nullptr;
  if (device->OpenPropertyStore(STGM_READ, &props) != S_OK) return std::nullopt;

  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = props->GetValue(key, &var);
  props->Release();

  if (hr != S_OK || var.vt != VT_LPWSTR) {
    PropVariantClear(&var);
    return std::nullopt;
  }

  QString value = QString::fromWCharArray(var.pwszVal);
  PropVariantClear(&var);
  return value;
}

} // namespace

struct Win32AudioControl::Impl {
  bool comInitialized = false;

  Impl() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    comInitialized = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
  }

  ~Impl() {
    if (comInitialized) CoUninitialize();
  }
};

Win32AudioControl::Win32AudioControl() : m_impl(std::make_unique<Impl>()) {}
Win32AudioControl::~Win32AudioControl() = default;

QString Win32AudioControl::id() const { return "wasapi"; }

float Win32AudioControl::getVolume() const {
  auto enumerator = createEnumerator();
  if (!enumerator) return 0.0f;

  auto device = defaultDevice(enumerator.get());
  if (!device) return 0.0f;

  auto volume = endpointVolume(device.get());
  if (!volume) return 0.0f;

  float level = 0.0f;
  if (volume->GetMasterVolumeLevelScalar(&level) != S_OK) return 0.0f;
  return std::clamp(level, 0.0f, 1.0f);
}

std::optional<float> Win32AudioControl::setVolume(float level) {
  auto enumerator = createEnumerator();
  if (!enumerator) return std::nullopt;

  auto device = defaultDevice(enumerator.get());
  if (!device) return std::nullopt;

  auto volume = endpointVolume(device.get());
  if (!volume) return std::nullopt;

  level = std::clamp(level, 0.0f, 1.0f);
  if (volume->SetMasterVolumeLevelScalar(level, nullptr) != S_OK) return std::nullopt;
  return level;
}

std::optional<float> Win32AudioControl::adjustVolume(float delta) { return setVolume(getVolume() + delta); }

bool Win32AudioControl::isMuted() const {
  auto enumerator = createEnumerator();
  if (!enumerator) return false;

  auto device = defaultDevice(enumerator.get());
  if (!device) return false;

  auto volume = endpointVolume(device.get());
  if (!volume) return false;

  BOOL muted = FALSE;
  if (volume->GetMute(&muted) != S_OK) return false;
  return muted != 0;
}

bool Win32AudioControl::setMuted(bool muted) {
  auto enumerator = createEnumerator();
  if (!enumerator) return false;

  auto device = defaultDevice(enumerator.get());
  if (!device) return false;

  auto volume = endpointVolume(device.get());
  if (!volume) return false;

  return volume->SetMute(muted ? TRUE : FALSE, nullptr) == S_OK;
}

bool Win32AudioControl::toggleMute() { return setMuted(!isMuted()); }

std::vector<AudioSink> Win32AudioControl::listSinks() const {
  auto enumerator = createEnumerator();
  if (!enumerator) return {};

  IMMDeviceCollection *collection = nullptr;
  if (enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection) != S_OK) return {};

  auto defaultDev = defaultDevice(enumerator.get());

  std::optional<QString> defaultId;
  if (defaultDev) {
    LPWSTR defaultIdRaw = nullptr;
    if (defaultDev->GetId(&defaultIdRaw) == S_OK) {
      defaultId = QString::fromWCharArray(defaultIdRaw);
      CoTaskMemFree(defaultIdRaw);
    }
  }

  UINT count = 0;
  collection->GetCount(&count);

  std::vector<AudioSink> sinks;
  sinks.reserve(count);

  for (UINT i = 0; i < count; ++i) {
    IMMDevice *dev = nullptr;
    if (collection->Item(i, &dev) != S_OK) continue;

    AudioSink sink;

    auto name = devicePropertyString(dev, PKEY_Device_FriendlyName);
    auto desc = devicePropertyString(dev, PKEY_DeviceInterface_FriendlyName);

    LPWSTR idRaw = nullptr;
    if (dev->GetId(&idRaw) == S_OK) {
      sink.name = name.value_or(QString::fromWCharArray(idRaw));
      sink.description = desc.value_or(sink.name);
      sink.isDefault = defaultId && QString::fromWCharArray(idRaw) == *defaultId;
      CoTaskMemFree(idRaw);
    } else {
      sink.name = name.value_or("Unknown");
      sink.description = desc.value_or(sink.name);
      sink.isDefault = false;
    }

    auto volume = endpointVolume(dev);
    if (volume) {
      float level = 0.0f;
      BOOL muted = FALSE;
      volume->GetMasterVolumeLevelScalar(&level);
      volume->GetMute(&muted);
      sink.volume = std::clamp(level, 0.0f, 1.0f);
      sink.muted = muted != 0;
    }

    sinks.emplace_back(std::move(sink));
    dev->Release();
  }

  collection->Release();
  return sinks;
}

bool Win32AudioControl::setDefaultSink(const QString &sinkName) {
  auto enumerator = createEnumerator();
  if (!enumerator) return false;

  IMMDeviceCollection *collection = nullptr;
  if (enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection) != S_OK) return false;

  UINT count = 0;
  collection->GetCount(&count);

  for (UINT i = 0; i < count; ++i) {
    IMMDevice *dev = nullptr;
    if (collection->Item(i, &dev) != S_OK) continue;

    auto name = devicePropertyString(dev, PKEY_Device_FriendlyName);
    if (name && *name == sinkName) {
      collection->Release();

      IPolicyConfig *policyConfig = nullptr;
      HRESULT hr =
          dev->Activate(IID_IPolicyConfig, CLSCTX_ALL, nullptr, reinterpret_cast<void **>(&policyConfig));
      if (SUCCEEDED(hr)) {
        hr = policyConfig->SetDefaultEndpoint(dev, nullptr);
        policyConfig->Release();
      }
      dev->Release();
      return SUCCEEDED(hr);
    }

    dev->Release();
  }

  collection->Release();
  return false;
}
