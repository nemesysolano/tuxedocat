#include "Files.h"
#include <cstdlib>

using namespace std;

namespace cli {
    vector<string> Files::listing(const string & directory) {
        const filesystem::path dir_path = filesystem::absolute(directory);

        if (!filesystem::exists(dir_path) || !filesystem::is_directory(dir_path)) {
            return {};
        }

        vector<string> data_file_paths_;
        for (const auto & entry : filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                data_file_paths_.push_back(filesystem::absolute(entry.path()).string());
            }
        }

        return data_file_paths_;
    }

    string Files::program_directory(char* argv[]) {
        if (argv == nullptr || argv[0] == nullptr || argv[0][0] == '\0') {
            return {};
        }

        std::filesystem::path p(argv[0]);

        if (p.is_absolute()) {
            std::error_code ec;
            auto resolved = std::filesystem::weakly_canonical(p, ec);
            return ec ? p.string() : resolved.string();
        }

        std::filesystem::path from_cwd = std::filesystem::current_path() / p;
        std::error_code ec;
        auto resolved = std::filesystem::weakly_canonical(from_cwd, ec);
        if (!ec) {
            return resolved.string();
        }

        const char* path_env = std::getenv("PATH");
        if (path_env != nullptr) {
            std::string paths = path_env;
            std::size_t start = 0;

            while (start <= paths.size()) {
                std::size_t end = paths.find(':', start);
                std::string dir = (end == std::string::npos)
                    ? paths.substr(start)
                    : paths.substr(start, end - start);

                if (dir.empty()) dir = ".";

                std::filesystem::path candidate = std::filesystem::path(dir) / p;
                std::error_code ec2;
                auto found = std::filesystem::weakly_canonical(candidate, ec2);

                if (!ec2 && std::filesystem::exists(found, ec2)) {
                    return found.string();
                }

                if (end == std::string::npos) break;
                start = end + 1;
            }
        }

        return p.string();
    }
}