/***

    Olive - Non-Linear Video Editor
    Copyright (C) 2019  Olive Team

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

***/

#include "gpu.h"

#include <QDebug>
#include <QDir>

#include <string>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>

// Both vendor drivers read these exported globals when the process creates its first OpenGL context. They have to
// exist even when the high performance adapter is not wanted, hence the zero defaults.
extern "C" {
  __declspec(dllexport) unsigned long NvOptimusEnablement = 0;
  __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
}
#endif

namespace olive {
namespace gpu {

namespace {

#ifdef Q_OS_WIN
/**
 * @brief Registry key Windows keeps per-application GPU preferences in
 *
 * Settings > System > Display > Graphics writes the same value, which is what makes the OS - and through it the
 * OpenGL drivers - pick a specific adapter for the application.
 */
const wchar_t* kGpuPreferencesKey = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";

/**
 * @brief Write (or remove, when value is null) the per-application preference value
 *
 * Done through the registry API rather than QSettings, because QSettings rewrites the backslashes of a Windows
 * path and the OS only matches the value under its native name.
 */
void SetGpuPreferenceValue(const QString &exe_path, const wchar_t* value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kGpuPreferencesKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr)
      != ERROR_SUCCESS) {
    qWarning() << "Could not open the per-application GPU preference registry key";
    return;
  }

  const std::wstring name = exe_path.toStdWString();
  if (value == nullptr) {
    RegDeleteValueW(key, name.c_str());
  } else {
    const std::wstring data(value);
    RegSetValueExW(key,
                   name.c_str(),
                   0,
                   REG_SZ,
                   reinterpret_cast<const BYTE*>(data.c_str()),
                   static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
  }

  RegCloseKey(key);
}

/**
 * @brief Guestimate whether an adapter is integrated
 *
 * Intel adapters always are, and an APU only reports a small slice of system memory as dedicated video memory
 * (an AMD APU typically reports 512 MB), where a real graphics card reports gigabytes.
 */
bool LooksIntegrated(quint32 vendor_id, quint64 memory) {
  return vendor_id == 0x8086 || memory < 1024ull * 1024ull * 1024ull;
}

/**
 * @brief Whether an adapter is a software rasteriser rather than a GPU
 *
 * DXGI reports the Microsoft Basic Render Driver, which nobody should be asked to render on.
 */
bool IsSoftwareAdapter(quint32 vendor_id, const QString& name) {
  return vendor_id == 0x1414 || name.contains("Basic Render", Qt::CaseInsensitive);
}
#endif

}

QVector<Device> EnumerateDevices() {
  QVector<Device> devices;

#ifdef Q_OS_WIN
  IDXGIFactory1* factory = nullptr;
  if (FAILED(CreateDXGIFactory1(IID_IDXGIFactory1, reinterpret_cast<void**>(&factory))) || factory == nullptr) {
    qWarning() << "Could not enumerate GPUs: DXGI factory creation failed";
    return devices;
  }

  for (UINT i = 0; ; i++) {
    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (adapter == nullptr) {
      continue;
    }

    DXGI_ADAPTER_DESC1 desc;
    if (SUCCEEDED(adapter->GetDesc1(&desc))) {
      Device dev;
      dev.name = QString::fromWCharArray(desc.Description).trimmed();
      dev.vendor_id = desc.VendorId;
      dev.memory = static_cast<quint64>(desc.DedicatedVideoMemory);
      dev.integrated = LooksIntegrated(dev.vendor_id, dev.memory);

      // DXGI can expose the same adapter more than once, and the software rasteriser is not a choice worth offering
      bool duplicate = false;
      for (int j=0;j<devices.size();j++) {
        if (devices.at(j).name == dev.name) {
          duplicate = true;
          break;
        }
      }
      if (!dev.name.isEmpty() && !duplicate && !IsSoftwareAdapter(dev.vendor_id, dev.name)) {
        devices.append(dev);
      }
    }

    adapter->Release();
  }

  factory->Release();
#endif

  return devices;
}

void ApplyPreference(const QString &device_name, const QString &exe_path) {
#ifdef Q_OS_WIN
  bool high_performance = false;
  bool device_present = device_name.isEmpty();

  const QVector<Device> devices = EnumerateDevices();

  if (!device_name.isEmpty()) {
    for (int i=0;i<devices.size();i++) {
      if (devices.at(i).name == device_name) {
        device_present = true;
        high_performance = !devices.at(i).integrated;
        break;
      }
    }

    if (!device_present) {
      qWarning() << "The chosen GPU" << device_name << "isn't present any more, using the default adapter";
    }
  }

  // Let the OS and the driver know which adapter this executable wants. An empty or stale name clears the entry so
  // the machine's own default applies again.
  if (!exe_path.isEmpty()) {
    // Qt hands out forward slashes; the registry has to see the native Windows form of the path
    const QString native_exe_path = QDir::toNativeSeparators(exe_path);
    if (device_present && !device_name.isEmpty()) {
      SetGpuPreferenceValue(native_exe_path, high_performance ? L"GpuPreference=2;" : L"GpuPreference=1;");
    } else {
      SetGpuPreferenceValue(native_exe_path, nullptr);
    }
  }

  NvOptimusEnablement = high_performance ? 1 : 0;
  AmdPowerXpressRequestHighPerformance = high_performance ? 1 : 0;

  qInfo() << "GPU preference: requested" << (device_name.isEmpty() ? QStringLiteral("automatic") : device_name)
          << "-> asking for" << (high_performance ? "the high performance adapter" : "the power saving adapter")
          << "(" << devices.size() << "adapters detected )";
#else
  Q_UNUSED(device_name);
  Q_UNUSED(exe_path);
#endif
}

} // namespace gpu
} // namespace olive
