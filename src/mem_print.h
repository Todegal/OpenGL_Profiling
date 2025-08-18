#pragma once

#include <string>

std::string humanReadableSize(const size_t sizeBytes)
{
        static constexpr std::string_view units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
        double size = static_cast<double>(sizeBytes);
        size_t unitIndex = 0;

        while (size >= 1024 && unitIndex < std::size(units) - 1)
        {
                unitIndex++;
                size /= 1024;
        }

        return std::format("{:.2f} {}", size, units[unitIndex]);
}
