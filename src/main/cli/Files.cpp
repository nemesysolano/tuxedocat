#include "Files.h"
#include <cstdlib>

using namespace std;

namespace cli {
    namespace {
        bool matches_filter(const string & name, const string & filter) {
            size_t name_index = 0;
            size_t filter_index = 0;
            size_t wildcard_index = string::npos;
            size_t retry_name_index = 0;

            while (name_index < name.size()) {
                if (filter_index < filter.size() &&
                    (filter[filter_index] == '?' || filter[filter_index] == name[name_index])) {
                    ++name_index;
                    ++filter_index;
                } else if (filter_index < filter.size() && filter[filter_index] == '*') {
                    wildcard_index = filter_index++;
                    retry_name_index = name_index;
                } else if (wildcard_index != string::npos) {
                    filter_index = wildcard_index + 1;
                    name_index = ++retry_name_index;
                } else {
                    return false;
                }
            }

            while (filter_index < filter.size() && filter[filter_index] == '*') {
                ++filter_index;
            }
            return filter_index == filter.size();
        }
    }

    vector<string> Files::listing(const string & directory) {
        return listing(directory, "*");
    }

    vector<string> Files::listing(const string & directory, const string & filter) {
        const filesystem::path dir_path = filesystem::absolute(directory);

        if (!filesystem::exists(dir_path) || !filesystem::is_directory(dir_path)) {
            return {};
        }

        vector<string> data_file_paths_;
        for (const auto & entry : filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && matches_filter(entry.path().filename().string(), filter)) {
                data_file_paths_.push_back(filesystem::absolute(entry.path()).string());
            }
        }

        return data_file_paths_;
    }

    bool Files::is_directory(const string & directory) {
       const filesystem::path dir_path = filesystem::absolute(directory);

        return (filesystem::exists(dir_path) && filesystem::is_directory(dir_path));
    }

    bool Files::is_regular_file(const string & path) {
        const filesystem::path file_path = filesystem::absolute(path);

        return (filesystem::exists(file_path) && filesystem::is_regular_file(file_path));
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