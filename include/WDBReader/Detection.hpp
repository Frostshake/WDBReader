#pragma once

#include "Utility.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace WDBReader::Detection {
	struct ClientInfo {
	public:
		std::string name;
		std::string locale;
		Utility::GameVersion version;
	};

    class Detector {
        public:
        using result_t = std::vector<ClientInfo>;
		using strategy_t = std::function<result_t(const std::filesystem::path&)>;

        Detector() = default;
		Detector(std::initializer_list<strategy_t> init);

        static Detector all();
        Detector& add(strategy_t&& strategy);
        result_t detect(const std::filesystem::path& directory) const;

        static result_t buildFile(const std::filesystem::path& directory);
		static result_t filesystemHints(const std::filesystem::path& directory);

        protected:
        std::vector<strategy_t> _strategies;
    };

}