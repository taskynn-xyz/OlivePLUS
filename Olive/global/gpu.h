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

#ifndef GPU_H
#define GPU_H

#include <QString>
#include <QVector>

namespace olive {
namespace gpu {

/**
 * @brief A graphics adapter the operating system reports
 */
struct Device {
  /**
   * @brief Human readable adapter name, e.g. "NVIDIA GeForce RTX 3060"
   */
  QString name;

  /**
   * @brief PCI vendor ID (0x10DE NVIDIA, 0x1002 AMD, 0x8086 Intel)
   */
  quint32 vendor_id;

  /**
   * @brief Dedicated video memory in bytes, 0 if the driver doesn't report it
   */
  quint64 memory;

  /**
   * @brief Whether this adapter looks like an integrated GPU
   */
  bool integrated;
};

/**
 * @brief List the graphics adapters that can render for this process
 *
 * Only implemented on Windows, through DXGI. Every other platform returns an empty list, because Qt's OpenGL
 * integration offers no way to pick an adapter there.
 */
QVector<Device> EnumerateDevices();

/**
 * @brief Ask the OS and the driver to run this process on a particular adapter
 *
 * @param device_name
 *
 * Device::name of the wanted adapter, or **EMPTY** to go back to the default behaviour
 *
 * @param exe_path
 *
 * Full path of the running executable, used as the key of the per-application preference
 *
 * Windows cannot make an OpenGL application create its context on a chosen adapter directly. What it can do is
 * store a per-application GPU preference (the same value Settings > System > Display > Graphics writes) and let
 * the vendor drivers honour it, with the NVIDIA Optimus and AMD PowerXpress request globals as a fallback for
 * drivers that predate that mechanism. Both are applied here, before the first OpenGL context exists.
 */
void ApplyPreference(const QString &device_name, const QString &exe_path);

} // namespace gpu
} // namespace olive

#endif // GPU_H
