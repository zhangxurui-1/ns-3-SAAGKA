#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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
    std::string out = "./trim-" + NowToString() + ".log";
    if (argc > 1)
    {
        out = std::string(argv[1]);
    }

    std::unordered_map<std::string, std::vector<double>> avg_datas;

    std::fstream fs(out, std::ios::out);
    std::string line;
    std::string event_name;
    while (std::getline(std::cin, line))
    {
        auto pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '+')
        {
            continue;
        }
        std::string subline = line.substr(pos);
        if ((subline.size() >= 6 && subline.substr(subline.size() - 6) == "events") ||
            (pos >= 3 && subline.substr(0, 3) == "Avg") || (pos < 3))
        {
            fs << line << std::endl;
        }
    }

    return 0;
}
