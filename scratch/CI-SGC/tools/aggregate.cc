#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string
NowToString()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

int
main(int argc, char* argv[])
{
    fs::path dir = "/Users/zxr/workspace/ns-3-dev/scratch/CI-SGC/log";

    fs::path out_file = dir / ("test_total_time_data_" + NowToString() + ".log");

    std::unordered_map<std::string, std::vector<double>> avg_datas;
    std::vector<fs::path> files;

    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        std::string filename = entry.path().filename().string();

        if (filename.rfind("test_total_time_security", 0) == 0)
        {
            files.push_back(entry.path());
        }
    }

    auto extract_security = [](const fs::path& p) {
        auto s = p.filename().string();
        auto pos = s.find("security");
        int l = 0;
        while (pos + 8 + l < s.size() && s[pos + 8 + l] >= '0' && s[pos + 8 + l] <= '9')
        {
            ++l;
        }
        return std::stoi(s.substr(pos + 8, l));
    };

    auto extract_size = [](const fs::path& p) {
        auto s = p.filename().string();
        auto pos = s.find("size");
        int l = 0;
        while (pos + 4 + l < s.size() && s[pos + 4 + l] >= '0' && s[pos + 4 + l] <= '9')
        {
            ++l;
        }
        return std::stoi(s.substr(pos + 4, l));
    };

    std::sort(files.begin(), files.end(), [&](const fs::path& a, const fs::path& b) {
        int sa = extract_security(a);
        int sb = extract_security(b);
        if (sa != sb)
        {
            return sa < sb;
        }
        return extract_size(a) < extract_size(b);
    });

    for (const auto& file : files)
    {
        std::string event_name;
        std::cout << "Processing file: " << file << std::endl;
        std::ifstream infs(file);
        std::string line;
        while (std::getline(infs, line))
        {
            auto pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos || line[pos] == '+')
            {
                continue;
            }
            std::string subline = line.substr(pos);
            if (subline.size() >= 6 && subline.substr(subline.size() - 6) == "events")
            {
                event_name = subline.substr(0, subline.find_first_of(':'));
            }
            else if (pos >= 3 && subline.substr(0, 3) == "Avg")
            {
                double avg = std::stod(subline.substr(4, subline.find_first_of(' ') - 4));
                avg_datas[event_name].push_back(avg / 1000);
            }
        }
    }

    std::ofstream outfs(out_file);
    for (const auto& [event_name, avgs] : avg_datas)
    {
        outfs << event_name << ":[";
        for (const auto& avg : avgs)
        {
            outfs << avg << ", ";
        }
        outfs << "]" << std::endl;
    }

    return 0;
}
